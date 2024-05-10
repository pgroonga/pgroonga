#include "pgroonga.h"

#include "pgrn-compatible.h"

#include "pgrn-global.h"
#include "pgrn-groonga.h"
#include "pgrn-index-status.h"
#include "pgrn-pg.h"
#include "pgrn-wal.h"
#include "pgrn-writable.h"

#include <access/tableam.h>
#include <catalog/pg_type.h>
#include <funcapi.h>
#include <miscadmin.h>

static bool PGrnWALEnabled = false;
static size_t PGrnWALMaxSize = 0;

bool
PGrnWALGetEnabled(void)
{
	return PGrnWALEnabled;
}

void
PGrnWALEnable(void)
{
	PGrnWALEnabled = true;
}

void
PGrnWALDisable(void)
{
	PGrnWALEnabled = false;
}

size_t
PGrnWALGetMaxSize(void)
{
	return PGrnWALMaxSize;
}

void
PGrnWALSetMaxSize(size_t size)
{
	PGrnWALMaxSize = size;
}

#ifdef PGRN_SUPPORT_WAL
#	include <access/generic_xlog.h>
#	include <access/heapam.h>
#	include <access/htup_details.h>
#	include <miscadmin.h>
#	include <storage/bufmgr.h>
#	include <storage/bufpage.h>
#	include <storage/lmgr.h>
#	include <storage/lockdefs.h>
#	include <utils/acl.h>
#	include <utils/builtins.h>

#	include <msgpack.h>
#endif

PGDLLEXPORT PG_FUNCTION_INFO_V1(pgroonga_wal_apply_index);
PGDLLEXPORT PG_FUNCTION_INFO_V1(pgroonga_wal_apply_all);
PGDLLEXPORT PG_FUNCTION_INFO_V1(pgroonga_wal_status);
PGDLLEXPORT PG_FUNCTION_INFO_V1(pgroonga_wal_truncate_index);
PGDLLEXPORT PG_FUNCTION_INFO_V1(pgroonga_wal_truncate_all);
PGDLLEXPORT PG_FUNCTION_INFO_V1(pgroonga_wal_set_applied_position_index);
PGDLLEXPORT PG_FUNCTION_INFO_V1(pgroonga_wal_set_applied_position_index_last);
PGDLLEXPORT PG_FUNCTION_INFO_V1(pgroonga_wal_set_applied_position_all);
PGDLLEXPORT PG_FUNCTION_INFO_V1(pgroonga_wal_set_applied_position_all_last);

#ifdef PGRN_SUPPORT_WAL
static struct PGrnBuffers *buffers = &PGrnBuffers;
#endif

#ifdef PGRN_SUPPORT_WAL
typedef enum
{
	PGRN_WAL_ACTION_INSERT,
	PGRN_WAL_ACTION_CREATE_TABLE,
	PGRN_WAL_ACTION_CREATE_COLUMN,
	PGRN_WAL_ACTION_SET_SOURCES,
	PGRN_WAL_ACTION_RENAME_TABLE,
	PGRN_WAL_ACTION_DELETE
} PGrnWALAction;

#	define PGRN_WAL_META_PAGE_SPECIAL_VERSION 1

typedef struct
{
	BlockNumber next;
	BlockNumber max;
	uint32_t version;
} PGrnWALMetaPageSpecial;

typedef struct
{
	GenericXLogState *state;
	PGrnWALMetaPageSpecial *metaPageSpecial;
	Buffer buffer;
	Page page;
} PGrnWALPageWriteData;
#endif

struct PGrnWALData_
{
	Relation index;
#ifdef PGRN_SUPPORT_WAL
	GenericXLogState *state;
	unsigned int nUsedPages;
	struct
	{
		Buffer buffer;
		Page page;
		PGrnWALMetaPageSpecial *pageSpecial;
	} meta;
	struct
	{
		Buffer buffer;
		Page page;
	} current;
	size_t nBuffers;
	Buffer buffers[MAX_GENERIC_XLOG_PAGES];
	msgpack_packer packer;
#endif
};

#ifdef PGRN_SUPPORT_WAL
static void
msgpack_pack_cstr(msgpack_packer *packer, const char *string)
{
	size_t size;

	size = strlen(string);
	msgpack_pack_str(packer, size);
	msgpack_pack_str_body(packer, string, size);
}

static void
msgpack_pack_grn_obj(msgpack_packer *packer, grn_obj *object)
{
	if (object)
	{
		if (grn_obj_is_text_family_bulk(ctx, object))
		{
			msgpack_pack_str(packer, GRN_TEXT_LEN(object));
			msgpack_pack_str_body(
				packer, GRN_TEXT_VALUE(object), GRN_TEXT_LEN(object));
		}
		else
		{
			char name[GRN_TABLE_MAX_KEY_SIZE];
			int nameSize;
			nameSize = grn_obj_name(ctx, object, name, GRN_TABLE_MAX_KEY_SIZE);
			msgpack_pack_str(packer, nameSize);
			msgpack_pack_str_body(packer, name, nameSize);
		}
	}
	else
	{
		msgpack_pack_nil(packer);
	}
}
#endif

#define PGRN_WAL_META_PAGE_BLOCK_NUMBER 0

#ifdef PGRN_SUPPORT_WAL
static Buffer
PGrnWALReadLockedBuffer(Relation index,
						BlockNumber blockNumber,
						int bufferLockMode)
{
	LOCKMODE lockMode = ExclusiveLock;
	Buffer buffer;

	if (blockNumber == P_NEW)
		LockRelationForExtension(index, lockMode);
	buffer = ReadBuffer(index, blockNumber);
	LockBuffer(buffer, bufferLockMode);
	if (blockNumber == P_NEW)
		UnlockRelationForExtension(index, lockMode);

	return buffer;
}

static char *
PGrnWALPageGetData(Page page)
{
	return PageGetContents(page);
}

static size_t
PGrnWALPageGetFreeSize(Page page)
{
	PageHeader pageHeader;

	pageHeader = (PageHeader) page;
	return pageHeader->pd_upper - pageHeader->pd_lower;
}

static LocationIndex
PGrnWALPageGetLastOffset(Page page)
{
	PageHeader pageHeader;

	pageHeader = (PageHeader) page;
	return pageHeader->pd_lower - SizeOfPageHeaderData;
}

static void
PGrnWALDataInitBuffers(PGrnWALData *data)
{
	size_t i;
	data->nBuffers = 0;
	for (i = 0; i < MAX_GENERIC_XLOG_PAGES; i++)
	{
		data->buffers[i] = InvalidBuffer;
	}
}

static void
PGrnWALDataReleaseBuffers(PGrnWALData *data)
{
	size_t i;
	for (i = 0; i < data->nBuffers; i++)
	{
		UnlockReleaseBuffer(data->buffers[i]);
		data->buffers[i] = InvalidBuffer;
	}
	data->nBuffers = 0;
}

static void
PGrnWALDataInitNUsedPages(PGrnWALData *data)
{
	data->nUsedPages = 1; /* meta page */
}

static void
PGrnWALDataInitMeta(PGrnWALData *data)
{
	if (RelationGetNumberOfBlocks(data->index) == 0)
	{
		data->meta.buffer =
			PGrnWALReadLockedBuffer(data->index, P_NEW, BUFFER_LOCK_EXCLUSIVE);
		data->buffers[data->nBuffers++] = data->meta.buffer;
		data->meta.page = GenericXLogRegisterBuffer(
			data->state, data->meta.buffer, GENERIC_XLOG_FULL_IMAGE);
		PageInit(data->meta.page, BLCKSZ, sizeof(PGrnWALMetaPageSpecial));
		data->meta.pageSpecial =
			(PGrnWALMetaPageSpecial *) PageGetSpecialPointer(data->meta.page);
		data->meta.pageSpecial->next = PGRN_WAL_META_PAGE_BLOCK_NUMBER + 1;
		data->meta.pageSpecial->max = data->meta.pageSpecial->next + 1;
		data->meta.pageSpecial->version = PGRN_WAL_META_PAGE_SPECIAL_VERSION;
	}
	else
	{
		data->meta.buffer =
			PGrnWALReadLockedBuffer(data->index,
									PGRN_WAL_META_PAGE_BLOCK_NUMBER,
									BUFFER_LOCK_EXCLUSIVE);
		data->buffers[data->nBuffers++] = data->meta.buffer;
		data->meta.page =
			GenericXLogRegisterBuffer(data->state, data->meta.buffer, 0);
		data->meta.pageSpecial =
			(PGrnWALMetaPageSpecial *) PageGetSpecialPointer(data->meta.page);
	}
}

static void
PGrnWALDataInitCurrent(PGrnWALData *data)
{
	data->current.buffer = InvalidBuffer;
	data->current.page = NULL;
}

static void
PGrnWALDataFinish(PGrnWALData *data)
{
	BlockNumber block;
	LocationIndex offset;
	if (data->current.page)
	{
		block = BufferGetBlockNumber(data->current.buffer);
		offset = PGrnWALPageGetLastOffset(data->current.page);
	}
	else
	{
		block = data->meta.pageSpecial->next;
		offset = 0;
	}
	GenericXLogFinish(data->state);
	PGrnIndexStatusSetWALAppliedPosition(data->index, block, offset);
}

static void
PGrnWALDataRestart(PGrnWALData *data)
{
	PGrnWALDataFinish(data);

	PGrnWALDataReleaseBuffers(data);

	data->state = GenericXLogStart(data->index);
	PGrnWALDataInitNUsedPages(data);
	PGrnWALDataInitMeta(data);
	PGrnWALDataInitCurrent(data);
}

static void
PGrnWALPageAppend(Page page, const char *data, size_t dataSize)
{
	PageHeader pageHeader;

	pageHeader = (PageHeader) page;
	memcpy(PGrnWALPageGetData(page) + PGrnWALPageGetLastOffset(page),
		   data,
		   dataSize);
	pageHeader->pd_lower += dataSize;
}

static void
PGrnWALPageFilled(PGrnWALData *data)
{
	data->current.page = NULL;
	data->current.buffer = InvalidBuffer;
	if (PGrnWALMaxSize == 0)
	{
		data->meta.pageSpecial->next++;
		if (data->meta.pageSpecial->next >= data->meta.pageSpecial->max)
			data->meta.pageSpecial->max = data->meta.pageSpecial->next + 1;
	}
	else
	{
		size_t currentSize =
			(1 /* meta */ + data->meta.pageSpecial->next) * BLCKSZ;
		size_t maxSize = PGrnWALMaxSize;
		size_t minMaxSize = (1 /* meta */ + 2 /* at least two data */) * BLCKSZ;
		if (maxSize < minMaxSize)
			maxSize = minMaxSize;
		if (currentSize < maxSize)
		{
			data->meta.pageSpecial->next++;
			if (data->meta.pageSpecial->next >= data->meta.pageSpecial->max)
				data->meta.pageSpecial->max = data->meta.pageSpecial->next + 1;
		}
		else
		{
			data->meta.pageSpecial->max = data->meta.pageSpecial->next + 1;
			data->meta.pageSpecial->next = PGRN_WAL_META_PAGE_BLOCK_NUMBER + 1;
		}
	}
}

static void
PGrnWALPageWriterEnsureCurrent(PGrnWALData *data)
{
	PGrnWALMetaPageSpecial *meta;

	if (!BufferIsInvalid(data->current.buffer))
		return;

	if (data->nUsedPages == MAX_GENERIC_XLOG_PAGES)
	{
		PGrnWALDataRestart(data);
	}

	meta = data->meta.pageSpecial;
	if (RelationGetNumberOfBlocks(data->index) <= meta->next)
	{
		data->current.buffer =
			PGrnWALReadLockedBuffer(data->index, P_NEW, BUFFER_LOCK_EXCLUSIVE);
		data->buffers[data->nBuffers++] = data->current.buffer;
		meta->next = BufferGetBlockNumber(data->current.buffer);
		data->current.page = GenericXLogRegisterBuffer(
			data->state, data->current.buffer, GENERIC_XLOG_FULL_IMAGE);
		PageInit(data->current.page, BLCKSZ, 0);
	}
	else
	{
		data->current.buffer = PGrnWALReadLockedBuffer(
			data->index, meta->next, BUFFER_LOCK_EXCLUSIVE);
		data->buffers[data->nBuffers++] = data->current.buffer;
		data->current.page =
			GenericXLogRegisterBuffer(data->state, data->current.buffer, 0);
		if (PGrnWALPageGetFreeSize(data->current.page) == 0)
			PageInit(data->current.page, BLCKSZ, 0);
	}

	data->nUsedPages++;
}

static int
PGrnWALPageWriter(void *userData, const char *buffer, size_t length)
{
	PGrnWALData *data = userData;
	int written = 0;
	size_t rest = length;

	while (written < length)
	{
		size_t freeSize;

		PGrnWALPageWriterEnsureCurrent(data);

		freeSize = PGrnWALPageGetFreeSize(data->current.page);
		if (rest <= freeSize)
		{
			PGrnWALPageAppend(data->current.page, buffer, rest);
			written += rest;
		}
		else
		{
			PGrnWALPageAppend(data->current.page, buffer, freeSize);
			written += freeSize;
			rest -= freeSize;
			buffer += freeSize;
		}

		if (PGrnWALPageGetFreeSize(data->current.page) == 0)
		{
			PGrnWALPageFilled(data);
			PGrnWALPageWriterEnsureCurrent(data);
		}
	}

	return written;
}

static void
PGrnWALDataInitMessagePack(PGrnWALData *data)
{
	msgpack_packer_init(&(data->packer), data, PGrnWALPageWriter);
}

static LOCKMODE
PGrnWALLockMode(void)
{
	if (RecoveryInProgress())
		return RowExclusiveLock;
	else
		return ShareUpdateExclusiveLock;
}

static BlockNumber
PGrnWALLockBlockNumber(void)
{
	/* We can use any block number for this. We just want an index
	 * level lock but we can't use LockRelation(index) because it
	 * conflicts with REINDEX INDEX CONCURRENTLY. */
	return 0;
}

static void
PGrnWALLock(Relation index)
{
	LockPage(index, PGrnWALLockBlockNumber(), PGrnWALLockMode());
}

static void
PGrnWALUnlock(Relation index)
{
	UnlockPage(index, PGrnWALLockBlockNumber(), PGrnWALLockMode());
}
#endif

PGrnWALData *
PGrnWALStart(Relation index)
{
#ifdef PGRN_SUPPORT_WAL
	PGrnWALData *data;

	if (!PGrnWALEnabled)
		return NULL;

	if (!RelationIsValid(index))
		return NULL;

	PGrnWALLock(index);

	data = palloc(sizeof(PGrnWALData));

	data->index = index;
	data->state = GenericXLogStart(data->index);

	PGrnWALDataInitBuffers(data);
	PGrnWALDataInitNUsedPages(data);
	PGrnWALDataInitMeta(data);
	PGrnWALDataInitCurrent(data);
	PGrnWALDataInitMessagePack(data);

	return data;
#else
	return NULL;
#endif
}

void
PGrnWALFinish(PGrnWALData *data)
{
#ifdef PGRN_SUPPORT_WAL
	if (!data)
		return;

	PGrnWALDataFinish(data);

	PGrnWALDataReleaseBuffers(data);

	PGrnWALUnlock(data->index);

	pfree(data);
#endif
}

void
PGrnWALAbort(PGrnWALData *data)
{
#ifdef PGRN_SUPPORT_WAL
	if (!data)
		return;

	GenericXLogAbort(data->state);

/* For PostgreSQL on Amazon Linux 2. PostgreSQL 12.8 or later provides this. */
#	ifndef INTERRUPTS_CAN_BE_PROCESSED
#		define INTERRUPTS_CAN_BE_PROCESSED() false
#	endif

	if (!INTERRUPTS_CAN_BE_PROCESSED())
	{
		PGrnWALDataReleaseBuffers(data);

		PGrnWALUnlock(data->index);
	}

	pfree(data);
#endif
}

void
PGrnWALInsertStart(PGrnWALData *data, grn_obj *table, size_t nColumns)
{
#ifdef PGRN_SUPPORT_WAL
	msgpack_packer *packer;
	size_t nElements = nColumns;

	if (!data)
		return;

	if (table)
		nElements++;

	packer = &(data->packer);
	msgpack_pack_map(packer, nElements);

	if (table)
	{
		char tableName[GRN_TABLE_MAX_KEY_SIZE];
		int tableNameSize;

		tableNameSize =
			grn_obj_name(ctx, table, tableName, GRN_TABLE_MAX_KEY_SIZE);
		msgpack_pack_cstr(packer, "_table");
		msgpack_pack_str(packer, tableNameSize);
		msgpack_pack_str_body(packer, tableName, tableNameSize);
	}
#endif
}

void
PGrnWALInsertFinish(PGrnWALData *data)
{
}

void
PGrnWALInsertColumnStart(PGrnWALData *data, const char *name, size_t nameSize)
{
#ifdef PGRN_SUPPORT_WAL
	msgpack_packer *packer;

	if (!data)
		return;

	packer = &(data->packer);

	msgpack_pack_str(packer, nameSize);
	msgpack_pack_str_body(packer, name, nameSize);
#endif
}

void
PGrnWALInsertColumnFinish(PGrnWALData *data)
{
}

#ifdef PGRN_SUPPORT_WAL
static void
PGrnWALInsertColumnValueRaw(PGrnWALData *data,
							const char *name,
							size_t nameSize,
							grn_id domain,
							const char *value,
							size_t valueSize)
{
	const char *tag = "[wal][insert][column][value]";
	msgpack_packer *packer;

	packer = &(data->packer);

	switch (domain)
	{
	case GRN_DB_BOOL:
		if (*((grn_bool *) value))
		{
			msgpack_pack_true(packer);
		}
		else
		{
			msgpack_pack_false(packer);
		}
		break;
	case GRN_DB_INT8:
		msgpack_pack_int8(packer, *((int8_t *) (value)));
		break;
	case GRN_DB_UINT8:
		msgpack_pack_uint8(packer, *((uint8_t *) (value)));
		break;
	case GRN_DB_INT16:
		msgpack_pack_int16(packer, *((int16_t *) (value)));
		break;
	case GRN_DB_UINT16:
		msgpack_pack_uint16(packer, *((uint16_t *) (value)));
		break;
	case GRN_DB_INT32:
		msgpack_pack_int32(packer, *((int32_t *) (value)));
		break;
	case GRN_DB_UINT32:
		msgpack_pack_uint32(packer, *((uint32_t *) (value)));
		break;
	case GRN_DB_INT64:
		msgpack_pack_int64(packer, *((int64_t *) (value)));
		break;
	case GRN_DB_UINT64:
		msgpack_pack_uint64(packer, *((uint64_t *) (value)));
		break;
	case GRN_DB_FLOAT:
		msgpack_pack_double(packer, *((double *) (value)));
		break;
	case GRN_DB_TIME:
		msgpack_pack_int64(packer, *((int64_t *) (value)));
		break;
	case GRN_DB_SHORT_TEXT:
	case GRN_DB_TEXT:
	case GRN_DB_LONG_TEXT:
		msgpack_pack_str(packer, valueSize);
		msgpack_pack_str_body(packer, value, valueSize);
		break;
	default:
	{
		char domainName[GRN_TABLE_MAX_KEY_SIZE];
		int domainNameSize;

		domainNameSize = grn_table_get_key(
			ctx, grn_ctx_db(ctx), domain, domainName, GRN_TABLE_MAX_KEY_SIZE);
		PGrnCheckRC(GRN_FUNCTION_NOT_IMPLEMENTED,
					"%s unsupported type: <%.*s>: <%.*s>",
					tag,
					(int) nameSize,
					name,
					domainNameSize,
					domainName);
	}
	break;
	}
}

static void
PGrnWALInsertColumnValueBulk(PGrnWALData *data,
							 const char *name,
							 size_t nameSize,
							 grn_obj *value)
{
	PGrnWALInsertColumnValueRaw(data,
								name,
								nameSize,
								value->header.domain,
								GRN_BULK_HEAD(value),
								GRN_BULK_VSIZE(value));
}

static void
PGrnWALInsertColumnValueVector(PGrnWALData *data,
							   const char *name,
							   size_t nameSize,
							   grn_obj *value)
{
	msgpack_packer *packer;
	unsigned int i, n;

	packer = &(data->packer);

	n = grn_vector_size(ctx, value);
	msgpack_pack_array(packer, n);
	for (i = 0; i < n; i++)
	{
		const char *element;
		unsigned int elementSize;
		grn_id domain;

		elementSize =
			grn_vector_get_element(ctx, value, i, &element, NULL, &domain);
		PGrnWALInsertColumnValueRaw(
			data, name, nameSize, domain, element, elementSize);
	}
}

static void
PGrnWALInsertColumnUValueVector(PGrnWALData *data,
								const char *name,
								size_t nameSize,
								grn_obj *value)
{
	msgpack_packer *packer;
	grn_id domain;
	unsigned int elementSize;
	unsigned int i, n;

	packer = &(data->packer);

	domain = value->header.domain;
	elementSize = grn_uvector_element_size(ctx, value);
	n = grn_uvector_size(ctx, value);
	msgpack_pack_array(packer, n);
	for (i = 0; i < n; i++)
	{
		const char *element;

		element = GRN_BULK_HEAD(value) + (elementSize * i);
		PGrnWALInsertColumnValueRaw(
			data, name, nameSize, domain, element, elementSize);
	}
}
#endif

void
PGrnWALInsertColumn(PGrnWALData *data, grn_obj *column, grn_obj *value)
{
#ifdef PGRN_SUPPORT_WAL
	const char *tag = "[wal][insert][column]";
	char name[GRN_TABLE_MAX_KEY_SIZE];
	int nameSize;

	if (!data)
		return;

	nameSize = grn_column_name(ctx, column, name, GRN_TABLE_MAX_KEY_SIZE);

	PGrnWALInsertColumnStart(data, name, nameSize);

	switch (value->header.type)
	{
	case GRN_BULK:
		PGrnWALInsertColumnValueBulk(data, name, nameSize, value);
		break;
	case GRN_VECTOR:
		PGrnWALInsertColumnValueVector(data, name, nameSize, value);
		break;
	case GRN_UVECTOR:
		PGrnWALInsertColumnUValueVector(data, name, nameSize, value);
		break;
	default:
		PGrnCheckRC(GRN_FUNCTION_NOT_IMPLEMENTED,
					"%s not bulk value isn't supported yet: <%.*s>: <%s>",
					tag,
					(int) nameSize,
					name,
					grn_obj_type_to_string(value->header.type));
		break;
	}

	PGrnWALInsertColumnFinish(data);
#endif
}

void
PGrnWALInsertKeyRaw(PGrnWALData *data, const void *key, size_t keySize)
{
#ifdef PGRN_SUPPORT_WAL
	msgpack_packer *packer;

	if (!data)
		return;

	packer = &(data->packer);

	PGrnWALInsertColumnStart(
		data, GRN_COLUMN_NAME_KEY, GRN_COLUMN_NAME_KEY_LEN);
	msgpack_pack_bin(packer, keySize);
	msgpack_pack_bin_body(packer, key, keySize);
	PGrnWALInsertColumnFinish(data);
#endif
}

void
PGrnWALInsertKey(PGrnWALData *data, grn_obj *key)
{
#ifdef PGRN_SUPPORT_WAL
	if (!data)
		return;

	PGrnWALInsertKeyRaw(data, GRN_BULK_HEAD(key), GRN_BULK_VSIZE(key));
#endif
}

void
PGrnWALCreateTable(Relation index,
				   const char *name,
				   size_t nameSize,
				   grn_table_flags flags,
				   grn_obj *type,
				   grn_obj *tokenizer,
				   grn_obj *normalizers,
				   grn_obj *tokenFilters)
{
#ifdef PGRN_SUPPORT_WAL
	PGrnWALData *data;
	msgpack_packer *packer;
	size_t nElements = 7;

	if (!name)
		return;

	data = PGrnWALStart(index);
	if (!data)
		return;

	packer = &(data->packer);
	msgpack_pack_map(packer, nElements);

	msgpack_pack_cstr(packer, "_action");
	msgpack_pack_uint32(packer, PGRN_WAL_ACTION_CREATE_TABLE);

	msgpack_pack_cstr(packer, "name");
	msgpack_pack_str(packer, nameSize);
	msgpack_pack_str_body(packer, name, nameSize);

	msgpack_pack_cstr(packer, "flags");
	msgpack_pack_uint32(packer, flags);

	msgpack_pack_cstr(packer, "type");
	msgpack_pack_grn_obj(packer, type);

	msgpack_pack_cstr(packer, "tokenizer");
	msgpack_pack_grn_obj(packer, tokenizer);

	msgpack_pack_cstr(packer, "normalizers");
	msgpack_pack_grn_obj(packer, normalizers);

	msgpack_pack_cstr(packer, "token_filters");
	msgpack_pack_grn_obj(packer, tokenFilters);

	PGrnWALFinish(data);
#endif
}

void
PGrnWALCreateColumn(Relation index,
					grn_obj *table,
					const char *name,
					size_t nameSize,
					grn_column_flags flags,
					grn_obj *type)
{
#ifdef PGRN_SUPPORT_WAL
	PGrnWALData *data;
	msgpack_packer *packer;
	size_t nElements = 5;

	if (!name)
		return;

	data = PGrnWALStart(index);
	if (!data)
		return;

	packer = &(data->packer);
	msgpack_pack_map(packer, nElements);

	msgpack_pack_cstr(packer, "_action");
	msgpack_pack_uint32(packer, PGRN_WAL_ACTION_CREATE_COLUMN);

	msgpack_pack_cstr(packer, "table");
	msgpack_pack_grn_obj(packer, table);

	msgpack_pack_cstr(packer, "name");
	msgpack_pack_str(packer, nameSize);
	msgpack_pack_str_body(packer, name, nameSize);

	msgpack_pack_cstr(packer, "flags");
	msgpack_pack_uint32(packer, flags);

	msgpack_pack_cstr(packer, "type");
	msgpack_pack_grn_obj(packer, type);

	PGrnWALFinish(data);
#endif
}

void
PGrnWALSetSourceIDs(Relation index, grn_obj *column, grn_obj *sourceIDs)
{
#ifdef PGRN_SUPPORT_WAL
	PGrnWALData *data;
	msgpack_packer *packer;
	size_t nElements = 3;

	data = PGrnWALStart(index);
	if (!data)
		return;

	packer = &(data->packer);
	msgpack_pack_map(packer, nElements);

	msgpack_pack_cstr(packer, "_action");
	msgpack_pack_uint32(packer, PGRN_WAL_ACTION_SET_SOURCES);

	msgpack_pack_cstr(packer, "column");
	msgpack_pack_grn_obj(packer, column);

	msgpack_pack_cstr(packer, "sources");
	{
		unsigned int i, nElements;

		nElements = GRN_BULK_VSIZE(sourceIDs) / sizeof(grn_id);
		msgpack_pack_array(packer, nElements);
		for (i = 0; i < nElements; i++)
		{
			grn_obj *source;
			source = grn_ctx_at(ctx, GRN_RECORD_VALUE_AT(sourceIDs, i));
			msgpack_pack_grn_obj(packer, source);
		}
	}

	PGrnWALFinish(data);
#endif
}

void
PGrnWALRenameTable(Relation index,
				   const char *name,
				   size_t nameSize,
				   const char *newName,
				   size_t newNameSize)
{
#ifdef PGRN_SUPPORT_WAL
	PGrnWALData *data;
	msgpack_packer *packer;
	size_t nElements = 3;

	data = PGrnWALStart(index);
	if (!data)
		return;

	packer = &(data->packer);
	msgpack_pack_map(packer, nElements);

	msgpack_pack_cstr(packer, "_action");
	msgpack_pack_uint32(packer, PGRN_WAL_ACTION_RENAME_TABLE);

	msgpack_pack_cstr(packer, "name");
	msgpack_pack_str(packer, nameSize);
	msgpack_pack_str_body(packer, name, nameSize);

	msgpack_pack_cstr(packer, "new_name");
	msgpack_pack_str(packer, newNameSize);
	msgpack_pack_str_body(packer, newName, newNameSize);

	PGrnWALFinish(data);
#endif
}

void
PGrnWALDelete(Relation index, grn_obj *table, const char *key, size_t keySize)
{
#ifdef PGRN_SUPPORT_WAL
	PGrnWALData *data;
	msgpack_packer *packer;
	size_t nElements = 3;

	data = PGrnWALStart(index);
	if (!data)
		return;

	packer = &(data->packer);
	msgpack_pack_map(packer, nElements);

	msgpack_pack_cstr(packer, "_action");
	msgpack_pack_uint32(packer, PGRN_WAL_ACTION_DELETE);

	msgpack_pack_cstr(packer, "table");
	msgpack_pack_grn_obj(packer, table);

	msgpack_pack_cstr(packer, "key");
	msgpack_pack_bin(packer, keySize);
	msgpack_pack_bin_body(packer, key, keySize);

	PGrnWALFinish(data);
#endif
}

#ifdef PGRN_SUPPORT_WAL
typedef struct
{
	Relation index;
	struct
	{
		BlockNumber block;
		LocationIndex offset;
	} current;
	grn_obj *sources;
} PGrnWALApplyData;

static bool
PGrnWALApplyNeeded(PGrnWALApplyData *data)
{
	BlockNumber currentBlock;
	LocationIndex currentOffset;
	BlockNumber nBlocks;

	PGrnIndexStatusGetWALAppliedPosition(
		data->index, &currentBlock, &currentOffset);
	if (currentBlock == PGRN_WAL_META_PAGE_BLOCK_NUMBER)
		currentBlock++;

	nBlocks = RelationGetNumberOfBlocks(data->index);
	if (currentBlock >= nBlocks)
	{
		return false;
	}
	else
	{
		Buffer buffer;
		Page page;
		LocationIndex offset;
		bool haveDataInCurrentPage;
		bool needToApply;

		buffer = PGrnWALReadLockedBuffer(
			data->index, currentBlock, BUFFER_LOCK_SHARE);
		page = BufferGetPage(buffer);
		offset = PGrnWALPageGetLastOffset(page);
		UnlockReleaseBuffer(buffer);
		haveDataInCurrentPage = (offset > currentOffset);
		if (haveDataInCurrentPage)
		{
			needToApply = true;
		}
		else
		{
			BlockNumber nextBlock;
			Buffer nextBuffer;
			Page nextPage;

			nextBlock = currentBlock + 1;
			if (nextBlock == nBlocks)
			{
				/* 0 is the meta page. 1 is the first page that has data. */
				nextBlock = 1;
			}
			nextBuffer = PGrnWALReadLockedBuffer(
				data->index, nextBlock, BUFFER_LOCK_SHARE);
			nextPage = BufferGetPage(nextBuffer);
			needToApply = (PGrnWALPageGetLastOffset(nextPage) > 0);
			UnlockReleaseBuffer(nextBuffer);
		}
		if (!needToApply)
			return false;
	}

	return PGrnIsWritable();
}

static bool
PGrnWALApplyKeyEqual(PGrnWALApplyData *data,
					 const char *context,
					 msgpack_object *key,
					 const char *name)
{
	const char *tag = "[wal][apply][key][equal]";
	size_t nameSize;

	if (key->type != MSGPACK_OBJECT_STR)
	{
		PGrnCheckRC(GRN_INVALID_ARGUMENT,
					"%s%s[%s(%u)]%skey must be string: <%#x>",
					tag,
					context ? context : "",
					RelationGetRelationName(data->index),
					RelationGetRelid(data->index),
					context ? " " : "",
					key->type);
	}

	nameSize = strlen(name);
	if (key->via.str.size != nameSize)
		return false;
	if (memcmp(key->via.str.ptr, name, nameSize) != 0)
		return false;

	return true;
}

static uint64_t
PGrnWALApplyValueGetPositiveInteger(PGrnWALApplyData *data,
									const char *context,
									msgpack_object_kv *kv)
{
	const char *tag = "[wal][apply][value][positive-integer][get]";
	if (kv->val.type != MSGPACK_OBJECT_POSITIVE_INTEGER)
	{
		PGrnCheckRC(GRN_INVALID_ARGUMENT,
					"%s%s[%s(%u)]%s%.*s value must be positive integer: <%#x>",
					tag,
					context ? context : "",
					RelationGetRelationName(data->index),
					RelationGetRelid(data->index),
					context ? " " : "",
					kv->key.via.str.size,
					kv->key.via.str.ptr,
					kv->val.type);
	}

	return kv->val.via.u64;
}

static void
PGrnWALApplyValueGetString(PGrnWALApplyData *data,
						   const char *context,
						   msgpack_object_kv *kv,
						   const char **string,
						   size_t *stringSize)
{
	const char *tag = "[wal][apply][value][string][get]";
	if (kv->val.type != MSGPACK_OBJECT_STR)
	{
		PGrnCheckRC(GRN_INVALID_ARGUMENT,
					"%s%s[%s(%u)]%s%.*s value must be string: <%#x>",
					tag,
					context ? context : "",
					RelationGetRelationName(data->index),
					RelationGetRelid(data->index),
					context ? " " : "",
					kv->key.via.str.size,
					kv->key.via.str.ptr,
					kv->val.type);
	}

	*string = kv->val.via.str.ptr;
	*stringSize = kv->val.via.str.size;
}

static void
PGrnWALApplyValueGetBinary(PGrnWALApplyData *data,
						   const char *context,
						   msgpack_object_kv *kv,
						   const char **binary,
						   size_t *binarySize)
{
	const char *tag = "[wal][apply][value][binary][get]";
	if (kv->val.type != MSGPACK_OBJECT_BIN)
	{
		PGrnCheckRC(GRN_INVALID_ARGUMENT,
					"%s%s[%s(%u)]%s%.*s value must be binary: <%#x>",
					tag,
					context ? context : "",
					RelationGetRelationName(data->index),
					RelationGetRelid(data->index),
					context ? " " : "",
					kv->key.via.bin.size,
					kv->key.via.bin.ptr,
					kv->val.type);
	}

	*binary = kv->val.via.bin.ptr;
	*binarySize = kv->val.via.bin.size;
}

static grn_obj *
PGrnWALApplyValueGetGroongaObject(PGrnWALApplyData *data,
								  const char *context,
								  msgpack_object_kv *kv)
{
	const char *tag = "[wal][apply][value][groonga-object][get]";
	grn_obj *object = NULL;

	switch (kv->val.type)
	{
	case MSGPACK_OBJECT_NIL:
		object = NULL;
		break;
	case MSGPACK_OBJECT_STR:
		object = PGrnLookupWithSize(
			kv->val.via.str.ptr, kv->val.via.str.size, ERROR);
		break;
	default:
		PGrnCheckRC(GRN_INVALID_ARGUMENT,
					"%s%s[%s(%u)]%s%.*s value must be nil or string: <%#x>",
					tag,
					context ? context : "",
					RelationGetRelationName(data->index),
					RelationGetRelid(data->index),
					context ? " " : "",
					kv->key.via.str.size,
					kv->key.via.str.ptr,
					kv->val.type);
		break;
	}

	return object;
}

static void
PGrnWALApplyValueGetGroongaObjectIDs(PGrnWALApplyData *data,
									 const char *context,
									 msgpack_object_kv *kv,
									 grn_obj *ids)
{
	const char *tag = "[wal][apply][value][groonga-object-ids][get]";
	msgpack_object_array *array;
	uint32_t i;

	if (kv->val.type != MSGPACK_OBJECT_ARRAY)
	{
		PGrnCheckRC(GRN_INVALID_ARGUMENT,
					"%s%s[%s(%u)]%s%.*s value must be array: <%#x>",
					tag,
					context ? context : "",
					RelationGetRelationName(data->index),
					RelationGetRelid(data->index),
					context ? " " : "",
					kv->key.via.str.size,
					kv->key.via.str.ptr,
					kv->val.type);
	}

	array = &(kv->val.via.array);
	for (i = 0; i < array->size; i++)
	{
		msgpack_object *element;
		grn_obj *object;
		grn_id objectID;

		element = &(array->ptr[i]);
		if (element->type != MSGPACK_OBJECT_STR)
		{
			PGrnCheckRC(GRN_INVALID_ARGUMENT,
						"%s%s[%s(%u)]%s%.*s "
						"value must be array of string: [%u]=<%#x>",
						tag,
						context ? context : "",
						RelationGetRelationName(data->index),
						RelationGetRelid(data->index),
						context ? " " : "",
						kv->key.via.str.size,
						kv->key.via.str.ptr,
						i,
						element->type);
		}

		object = PGrnLookupWithSize(
			element->via.str.ptr, element->via.str.size, ERROR);
		objectID = grn_obj_id(ctx, object);
		GRN_RECORD_PUT(ctx, ids, objectID);
	}
}

static grn_obj *
PGrnWALApplyValueGetTableModule(PGrnWALApplyData *data,
								const char *context,
								msgpack_object_kv *kv,
								grn_obj *buffer)
{
	const char *tag = "[wal][apply][value][table-module][get]";
	grn_obj *module = NULL;

	switch (kv->val.type)
	{
	case MSGPACK_OBJECT_NIL:
		break;
	case MSGPACK_OBJECT_STR:
		module = buffer;
		GRN_BULK_REWIND(module);
		GRN_TEXT_SET(ctx, module, kv->val.via.str.ptr, kv->val.via.str.size);
		break;
	default:
		PGrnCheckRC(GRN_INVALID_ARGUMENT,
					"%s%s[%s(%u)]%s%.*s value must be nil or string: <%#x>",
					tag,
					context ? context : "",
					RelationGetRelationName(data->index),
					RelationGetRelid(data->index),
					context ? " " : "",
					kv->key.via.str.size,
					kv->key.via.str.ptr,
					kv->val.type);
		break;
	}

	return module;
}

static grn_obj *
PGrnWALApplyValueGetTableModules(PGrnWALApplyData *data,
								 const char *context,
								 msgpack_object_kv *kv,
								 grn_obj *buffer)
{
	const char *tag = "[wal][apply][value][table-modules][get]";
	grn_obj *modules = NULL;

	switch (kv->val.type)
	{
	case MSGPACK_OBJECT_NIL:
		break;
	case MSGPACK_OBJECT_STR:
		modules = buffer;
		GRN_BULK_REWIND(modules);
		GRN_TEXT_SET(ctx, modules, kv->val.via.str.ptr, kv->val.via.str.size);
		break;
	case MSGPACK_OBJECT_ARRAY:
	{
		msgpack_object_array *array;
		uint32_t i;

		GRN_BULK_REWIND(modules);
		array = &(kv->val.via.array);
		for (i = 0; i < array->size; i++)
		{
			msgpack_object *element;

			element = &(array->ptr[i]);
			if (element->type != MSGPACK_OBJECT_STR)
			{
				PGrnCheckRC(GRN_INVALID_ARGUMENT,
							"%s%s[%s(%u)]%s"
							"%.*s value must be string or array of string: "
							"[%u]=<%#x>",
							tag,
							context ? context : "",
							RelationGetRelationName(data->index),
							RelationGetRelid(data->index),
							context ? " " : "",
							kv->key.via.str.size,
							kv->key.via.str.ptr,
							i,
							element->type);
			}

			if (i > 0)
				GRN_TEXT_PUTS(ctx, modules, ", ");
			GRN_TEXT_PUT(
				ctx, modules, element->via.str.ptr, element->via.str.size);
		}
		break;
	}
	default:
		PGrnCheckRC(GRN_INVALID_ARGUMENT,
					"%s%s[%s(%u)]%s%.*s value must be string or array: <%#x>",
					tag,
					context ? context : "",
					RelationGetRelationName(data->index),
					RelationGetRelid(data->index),
					context ? " " : "",
					kv->key.via.str.size,
					kv->key.via.str.ptr,
					kv->val.type);
		break;
	}

	return modules;
}

static void
PGrnWALApplyInsertArray(PGrnWALApplyData *data,
						msgpack_object_array *array,
						grn_obj *value,
						grn_id range_id)
{
	const char *tag = "[wal][apply][insert][array]";
	grn_obj *range;
	grn_id element_domain_id;
	uint32_t i;

	range = grn_ctx_at(ctx, range_id);
	if (grn_obj_is_lexicon(ctx, range))
	{
		element_domain_id = range->header.domain;
	}
	else
	{
		element_domain_id = range_id;
	}
	grn_obj_reinit(ctx, value, element_domain_id, GRN_OBJ_VECTOR);

	for (i = 0; i < array->size; i++)
	{
		msgpack_object *element;

		element = &(array->ptr[i]);
		switch (element->type)
		{
		case MSGPACK_OBJECT_BOOLEAN:
			GRN_BOOL_PUT(ctx, value, element->via.boolean);
			break;
		case MSGPACK_OBJECT_POSITIVE_INTEGER:
		case MSGPACK_OBJECT_NEGATIVE_INTEGER:
#	define ELEMENT_VALUE                                                      \
		(element->type == MSGPACK_OBJECT_POSITIVE_INTEGER ? element->via.u64   \
														  : element->via.i64)
			switch (element_domain_id)
			{
			case GRN_DB_INT8:
				GRN_INT8_PUT(ctx, value, ELEMENT_VALUE);
				break;
			case GRN_DB_UINT8:
				GRN_UINT8_PUT(ctx, value, ELEMENT_VALUE);
				break;
			case GRN_DB_INT16:
				GRN_INT16_PUT(ctx, value, ELEMENT_VALUE);
				break;
			case GRN_DB_UINT16:
				GRN_UINT16_PUT(ctx, value, ELEMENT_VALUE);
				break;
			case GRN_DB_INT32:
				GRN_INT32_PUT(ctx, value, ELEMENT_VALUE);
				break;
			case GRN_DB_UINT32:
				GRN_UINT32_PUT(ctx, value, ELEMENT_VALUE);
				break;
			case GRN_DB_INT64:
				GRN_INT64_PUT(ctx, value, ELEMENT_VALUE);
				break;
			case GRN_DB_UINT64:
				GRN_UINT64_PUT(ctx, value, ELEMENT_VALUE);
				break;
			default:
			{
				grn_obj key;
				if (element->type == MSGPACK_OBJECT_POSITIVE_INTEGER)
				{
					GRN_INT64_INIT(&key, 0);
					GRN_INT64_SET(ctx, &key, element->via.i64);
				}
				else
				{
					GRN_UINT64_INIT(&key, 0);
					GRN_UINT64_SET(ctx, &key, element->via.u64);
				}
				grn_obj_cast(ctx, &key, value, GRN_FALSE);
				GRN_OBJ_FIN(ctx, &key);
			}
			break;
			}
			break;
#	undef ELEMENT_VALUE
		case MSGPACK_OBJECT_FLOAT:
			GRN_FLOAT_PUT(ctx, value, element->via.f64);
			break;
		case MSGPACK_OBJECT_STR:
			grn_vector_add_element(ctx,
								   value,
								   element->via.str.ptr,
								   element->via.str.size,
								   0,
								   element_domain_id);
			break;
		default:
			PGrnCheckRC(GRN_INVALID_ARGUMENT,
						"%s[%s(%u)] unexpected element type: <%#x>",
						tag,
						RelationGetRelationName(data->index),
						RelationGetRelid(data->index),
						element->type);
			break;
		}
	}
}

static void
PGrnWALApplyInsert(PGrnWALApplyData *data,
				   msgpack_object_map *map,
				   uint32_t currentElement)
{
	const char *tag = "[wal][apply]";
	const char *context = "[insert]";
	grn_obj *table = NULL;
	const char *key = NULL;
	size_t keySize = 0;
	grn_id id;
	uint32_t i;

	if (currentElement < map->size)
	{
		msgpack_object_kv *kv;

		kv = &(map->ptr[currentElement]);
		if (PGrnWALApplyKeyEqual(data, context, &(kv->key), "_table"))
		{
			table = PGrnWALApplyValueGetGroongaObject(data, context, kv);
			currentElement++;
		}
	}
	if (!table)
	{
		if (!data->sources)
			data->sources = PGrnLookupSourcesTable(data->index, ERROR);
		table = data->sources;
	}

	if (currentElement < map->size)
	{
		msgpack_object_kv *kv;

		kv = &(map->ptr[currentElement]);
		if (PGrnWALApplyKeyEqual(
				data, context, &(kv->key), GRN_COLUMN_NAME_KEY))
		{
			PGrnWALApplyValueGetBinary(data, context, kv, &key, &keySize);
			currentElement++;
		}
	}

	id = grn_table_add(ctx, table, key, keySize, NULL);
	for (i = currentElement; i < map->size; i++)
	{
		msgpack_object *key;
		msgpack_object *value;
		grn_obj *column;
		grn_obj *walValue = &(buffers->walValue);

		key = &(map->ptr[i].key);
		value = &(map->ptr[i].val);

		if (key->type != MSGPACK_OBJECT_STR)
		{
			PGrnCheckRC(GRN_INVALID_ARGUMENT,
						"%s%s[%s(%u)] key must be map: <%#x>",
						tag,
						context,
						RelationGetRelationName(data->index),
						RelationGetRelid(data->index),
						key->type);
		}

		column = PGrnLookupColumnWithSize(
			table, key->via.str.ptr, key->via.str.size, ERROR);
		switch (value->type)
		{
		case MSGPACK_OBJECT_BOOLEAN:
			grn_obj_reinit(ctx, walValue, GRN_DB_BOOL, 0);
			GRN_BOOL_SET(ctx, walValue, value->via.boolean);
			break;
		case MSGPACK_OBJECT_POSITIVE_INTEGER:
			grn_obj_reinit(ctx, walValue, GRN_DB_UINT64, 0);
			GRN_UINT64_SET(ctx, walValue, value->via.u64);
			break;
		case MSGPACK_OBJECT_NEGATIVE_INTEGER:
			grn_obj_reinit(ctx, walValue, GRN_DB_INT64, 0);
			GRN_INT64_SET(ctx, walValue, value->via.i64);
			break;
		case MSGPACK_OBJECT_FLOAT:
			grn_obj_reinit(ctx, walValue, GRN_DB_FLOAT, 0);
			GRN_FLOAT_SET(ctx, walValue, value->via.f64);
			break;
		case MSGPACK_OBJECT_STR:
			grn_obj_reinit(ctx, walValue, GRN_DB_TEXT, 0);
			GRN_TEXT_SET(
				ctx, walValue, value->via.str.ptr, value->via.str.size);
			break;
		case MSGPACK_OBJECT_ARRAY:
			PGrnWALApplyInsertArray(data,
									&(value->via.array),
									walValue,
									grn_obj_get_range(ctx, column));
			break;
			/*
					case MSGPACK_OBJECT_MAP:
						break;
					case MSGPACK_OBJECT_BIN:
						break;
					case MSGPACK_OBJECT_EXT:
						break;
			*/
		default:
			PGrnCheckRC(GRN_INVALID_ARGUMENT,
						"%s%s[%s(%u)] unexpected value type: <%#x>",
						tag,
						context,
						RelationGetRelationName(data->index),
						RelationGetRelid(data->index),
						value->type);
			break;
		}
		grn_obj_set_value(ctx, column, id, walValue, GRN_OBJ_SET);
	}
}

static void
PGrnWALApplyCreateTable(PGrnWALApplyData *data,
						msgpack_object_map *map,
						uint32_t currentElement)
{
	const char *context = "[create-table]";
	const char *name = NULL;
	size_t nameSize = 0;
	grn_table_flags flags = 0;
	grn_obj *type = NULL;
	grn_obj *tokenizer = NULL;
	grn_obj *normalizers = NULL;
	grn_obj *tokenFilters = NULL;
	uint32_t i;

	for (i = currentElement; i < map->size; i++)
	{
		msgpack_object_kv *kv;

		kv = &(map->ptr[i]);
		if (PGrnWALApplyKeyEqual(data, context, &(kv->key), "name"))
		{
			PGrnWALApplyValueGetString(data, context, kv, &name, &nameSize);
		}
		else if (PGrnWALApplyKeyEqual(data, context, &(kv->key), "flags"))
		{
			flags = PGrnWALApplyValueGetPositiveInteger(data, context, kv);
		}
		else if (PGrnWALApplyKeyEqual(data, context, &(kv->key), "type"))
		{
			type = PGrnWALApplyValueGetGroongaObject(data, context, kv);
		}
		else if (PGrnWALApplyKeyEqual(data, context, &(kv->key), "tokenizer"))
		{
			tokenizer = PGrnWALApplyValueGetTableModule(
				data, context, kv, &(buffers->tokenizer));
		}
		else if (PGrnWALApplyKeyEqual(
					 data, context, &(kv->key), "normalizer") ||
				 PGrnWALApplyKeyEqual(data, context, &(kv->key), "normalizers"))
		{
			normalizers = PGrnWALApplyValueGetTableModule(
				data, context, kv, &(buffers->normalizers));
		}
		else if (PGrnWALApplyKeyEqual(
					 data, context, &(kv->key), "token_filters"))
		{
			tokenFilters = PGrnWALApplyValueGetTableModules(
				data, context, kv, &(buffers->tokenFilters));
		}
	}

	PGrnCreateTableWithSize(NULL,
							name,
							nameSize,
							flags,
							type,
							tokenizer,
							normalizers,
							tokenFilters);
}

static void
PGrnWALApplyCreateColumn(PGrnWALApplyData *data,
						 msgpack_object_map *map,
						 uint32_t currentElement)
{
	const char *context = "[create-column]";
	grn_obj *table = NULL;
	const char *name = NULL;
	size_t nameSize = 0;
	grn_column_flags flags = 0;
	grn_obj *type = NULL;
	uint32_t i;

	for (i = currentElement; i < map->size; i++)
	{
		msgpack_object_kv *kv;

		kv = &(map->ptr[i]);
		if (PGrnWALApplyKeyEqual(data, context, &(kv->key), "table"))
		{
			table = PGrnWALApplyValueGetGroongaObject(data, context, kv);
		}
		else if (PGrnWALApplyKeyEqual(data, context, &(kv->key), "name"))
		{
			PGrnWALApplyValueGetString(data, context, kv, &name, &nameSize);
		}
		else if (PGrnWALApplyKeyEqual(data, context, &(kv->key), "flags"))
		{
			flags = PGrnWALApplyValueGetPositiveInteger(data, context, kv);
		}
		else if (PGrnWALApplyKeyEqual(data, context, &(kv->key), "type"))
		{
			type = PGrnWALApplyValueGetGroongaObject(data, context, kv);
		}
	}

	PGrnCreateColumnWithSize(NULL, table, name, nameSize, flags, type);
}

static void
PGrnWALApplySetSources(PGrnWALApplyData *data,
					   msgpack_object_map *map,
					   uint32_t currentElement)
{
	const char *context = "[set-sources]";
	grn_obj *column = NULL;
	grn_obj *sourceIDs = &(buffers->sourceIDs);
	uint32_t i;

	GRN_BULK_REWIND(sourceIDs);
	for (i = currentElement; i < map->size; i++)
	{
		msgpack_object_kv *kv;

		kv = &(map->ptr[i]);
		if (PGrnWALApplyKeyEqual(data, context, &(kv->key), "column"))
		{
			column = PGrnWALApplyValueGetGroongaObject(data, context, kv);
		}
		else if (PGrnWALApplyKeyEqual(data, context, &(kv->key), "sources"))
		{
			PGrnWALApplyValueGetGroongaObjectIDs(data, context, kv, sourceIDs);
		}
	}

	grn_obj_set_info(ctx, column, GRN_INFO_SOURCE, sourceIDs);
}

static void
PGrnWALApplyRenameTable(PGrnWALApplyData *data,
						msgpack_object_map *map,
						uint32_t currentElement)
{
	const char *context = "[rename-table]";
	grn_obj *table = NULL;
	const char *newName = NULL;
	size_t newNameSize = 0;
	uint32_t i;

	for (i = currentElement; i < map->size; i++)
	{
		msgpack_object_kv *kv;

		kv = &(map->ptr[i]);
		if (PGrnWALApplyKeyEqual(data, context, &(kv->key), "name"))
		{
			table = PGrnWALApplyValueGetGroongaObject(data, context, kv);
		}
		else if (PGrnWALApplyKeyEqual(data, context, &(kv->key), "new_name"))
		{
			PGrnWALApplyValueGetString(
				data, context, kv, &newName, &newNameSize);
		}
	}

	grn_table_rename(ctx, table, newName, newNameSize);
}

static void
PGrnWALApplyDelete(PGrnWALApplyData *data,
				   msgpack_object_map *map,
				   uint32_t currentElement)
{
	const char *context = "[delete]";
	grn_obj *table = NULL;
	const char *key = NULL;
	size_t keySize = 0;
	uint32_t i;

	for (i = currentElement; i < map->size; i++)
	{
		msgpack_object_kv *kv;

		kv = &(map->ptr[i]);
		if (PGrnWALApplyKeyEqual(data, context, &(kv->key), "table"))
		{
			table = PGrnWALApplyValueGetGroongaObject(data, context, kv);
		}
		else if (PGrnWALApplyKeyEqual(data, context, &(kv->key), "key"))
		{
			PGrnWALApplyValueGetBinary(data, context, kv, &key, &keySize);
			currentElement++;
		}
	}

	if (table->header.type == GRN_TABLE_NO_KEY)
	{
		const uint64_t packedCtid = *((uint64_t *) key);
		grn_obj *ctidColumn =
			grn_obj_column(ctx, table, "ctid", strlen("ctid"));
		grn_obj ctidValue;
		GRN_UINT64_INIT(&ctidValue, 0);
		GRN_TABLE_EACH_BEGIN(ctx, table, cursor, id)
		{
			GRN_BULK_REWIND(&ctidValue);
			grn_obj_get_value(ctx, ctidColumn, id, &ctidValue);
			if (packedCtid == GRN_UINT64_VALUE(&ctidValue))
			{
				grn_table_cursor_delete(ctx, cursor);
				break;
			}
		}
		GRN_TABLE_EACH_END(ctx, cursor);
		GRN_OBJ_FIN(ctx, &ctidValue);
		grn_obj_unlink(ctx, ctidColumn);
	}
	else
	{
		grn_table_delete(ctx, table, key, keySize);
	}
}

static void
PGrnWALApplyObject(PGrnWALApplyData *data, msgpack_object *object)
{
	const char *tag = "[wal][apply][object]";
	const char *context = NULL;
	msgpack_object_map *map;
	uint32_t currentElement = 0;
	PGrnWALAction action = PGRN_WAL_ACTION_INSERT;

	if (object->type != MSGPACK_OBJECT_MAP)
	{
		const char *message = "record must be map";
		BlockNumber currentBlock;
		LocationIndex currentOffset;

		PGrnIndexStatusGetWALAppliedPosition(
			data->index, &currentBlock, &currentOffset);
		switch (object->type)
		{
		case MSGPACK_OBJECT_NIL:
			PGrnCheckRC(GRN_INVALID_ARGUMENT,
						"%s[%s(%u)] %s: <%u><%u>: <nil>",
						tag,
						RelationGetRelationName(data->index),
						RelationGetRelid(data->index),
						message,
						currentBlock,
						currentOffset);
			break;
		case MSGPACK_OBJECT_BOOLEAN:
			PGrnCheckRC(GRN_INVALID_ARGUMENT,
						"%s[%s(%u)] %s: <%u><%u>: <boolean>: <%s>",
						tag,
						RelationGetRelationName(data->index),
						RelationGetRelid(data->index),
						message,
						currentBlock,
						currentOffset,
						object->via.boolean ? "true" : "false");
			break;
		case MSGPACK_OBJECT_POSITIVE_INTEGER:
			PGrnCheckRC(GRN_INVALID_ARGUMENT,
						"%s[%s(%u)] %s: <%u><%u>: "
						"<positive-integer>: <%" PRId64 ">",
						tag,
						RelationGetRelationName(data->index),
						RelationGetRelid(data->index),
						message,
						currentBlock,
						currentOffset,
						object->via.i64);
			break;
		case MSGPACK_OBJECT_NEGATIVE_INTEGER:
			PGrnCheckRC(GRN_INVALID_ARGUMENT,
						"%s[%s(%u)] %s: <%u><%u>: "
						"<negative-integer>: <%" PRIu64 ">",
						tag,
						RelationGetRelationName(data->index),
						RelationGetRelid(data->index),
						message,
						currentBlock,
						currentOffset,
						object->via.u64);
			break;
		case MSGPACK_OBJECT_FLOAT:
			PGrnCheckRC(GRN_INVALID_ARGUMENT,
						"%s[%s(%u)] %s: <%u><%u>: <float>: <%g>",
						tag,
						RelationGetRelationName(data->index),
						RelationGetRelid(data->index),
						message,
						currentBlock,
						currentOffset,
						object->via.f64);
			break;
		case MSGPACK_OBJECT_STR:
			PGrnCheckRC(GRN_INVALID_ARGUMENT,
						"%s[%s(%u)] %s: <%u><%u>: <string>: <%.*s>",
						tag,
						RelationGetRelationName(data->index),
						RelationGetRelid(data->index),
						message,
						currentBlock,
						currentOffset,
						(int) object->via.str.size,
						object->via.str.ptr);
			break;
		case MSGPACK_OBJECT_ARRAY:
			PGrnCheckRC(GRN_INVALID_ARGUMENT,
						"%s[%s(%u)] %s: <%u><%u>: <array>: <%u>",
						tag,
						RelationGetRelationName(data->index),
						RelationGetRelid(data->index),
						message,
						currentBlock,
						currentOffset,
						object->via.array.size);
			break;
#	if MSGPACK_VERSION_MAJOR != 0
		case MSGPACK_OBJECT_BIN:
			PGrnCheckRC(GRN_INVALID_ARGUMENT,
						"%s[%s(%u)] %s: <%u><%u>: <binary>: <%u>",
						tag,
						RelationGetRelationName(data->index),
						RelationGetRelid(data->index),
						message,
						currentBlock,
						currentOffset,
						object->via.bin.size);
			break;
#	endif
		default:
			PGrnCheckRC(GRN_INVALID_ARGUMENT,
						"%s[%s(%u)] %s: <%u><%u>: <%#x>",
						tag,
						RelationGetRelationName(data->index),
						RelationGetRelid(data->index),
						message,
						currentBlock,
						currentOffset,
						object->type);
			break;
		}
	}

	map = &(object->via.map);

	if (currentElement < map->size)
	{
		msgpack_object_kv *kv;

		kv = &(object->via.map.ptr[currentElement]);
		if (PGrnWALApplyKeyEqual(data, context, &(kv->key), "_action"))
		{
			action = PGrnWALApplyValueGetPositiveInteger(data, context, kv);
			currentElement++;
		}
	}

	switch (action)
	{
	case PGRN_WAL_ACTION_INSERT:
		PGrnWALApplyInsert(data, map, currentElement);
		break;
	case PGRN_WAL_ACTION_CREATE_TABLE:
		PGrnWALApplyCreateTable(data, map, currentElement);
		break;
	case PGRN_WAL_ACTION_CREATE_COLUMN:
		PGrnWALApplyCreateColumn(data, map, currentElement);
		break;
	case PGRN_WAL_ACTION_SET_SOURCES:
		PGrnWALApplySetSources(data, map, currentElement);
		break;
	case PGRN_WAL_ACTION_RENAME_TABLE:
		PGrnWALApplyRenameTable(data, map, currentElement);
		break;
	case PGRN_WAL_ACTION_DELETE:
		PGrnWALApplyDelete(data, map, currentElement);
		break;
	default:
		PGrnCheckRC(GRN_INVALID_ARGUMENT,
					"%s[%s(%u)] unexpected action: <%d>",
					tag,
					RelationGetRelationName(data->index),
					RelationGetRelid(data->index),
					action);
		break;
	}
}

static int64_t
PGrnWALApplyConsume(PGrnWALApplyData *data)
{
	int64_t nAppliedOperations = 0;
	Buffer metaBuffer;
	Page metaPage;
	PGrnWALMetaPageSpecial *meta;
	BlockNumber i;
	BlockNumber startBlock;
	LocationIndex dataOffset;
	BlockNumber nBlocks;
	BlockNumber nextBlock;
	BlockNumber maxBlock;
	msgpack_unpacker unpacker;
	msgpack_unpacked unpacked;
	size_t bufferedSize = 0;

	msgpack_unpacker_init(&unpacker, MSGPACK_UNPACKER_INIT_BUFFER_SIZE);
	msgpack_unpacked_init(&unpacked);
	metaBuffer = PGrnWALReadLockedBuffer(
		data->index, PGRN_WAL_META_PAGE_BLOCK_NUMBER, BUFFER_LOCK_SHARE);
	metaPage = BufferGetPage(metaBuffer);
	meta = (PGrnWALMetaPageSpecial *) PageGetSpecialPointer(metaPage);
	startBlock = data->current.block;
	dataOffset = data->current.offset;
	if (startBlock == PGRN_WAL_META_PAGE_BLOCK_NUMBER)
		startBlock++;
	nBlocks = RelationGetNumberOfBlocks(data->index);
	nextBlock = meta->next;
	maxBlock = meta->max;
	if (maxBlock == PGRN_WAL_META_PAGE_BLOCK_NUMBER)
		maxBlock = nBlocks;
	PG_TRY();
	{
		for (i = 0; i < nBlocks; i++)
		{
			BlockNumber block;
			Buffer buffer;
			Page page;
			LocationIndex lastOffset;
			size_t dataSize;
			size_t parsedSize;

			block = (startBlock + i) % nBlocks;
			if (block == PGRN_WAL_META_PAGE_BLOCK_NUMBER)
				continue;
			if (block > maxBlock)
				continue;

			buffer =
				PGrnWALReadLockedBuffer(data->index, block, BUFFER_LOCK_SHARE);
			page = BufferGetPage(buffer);
			lastOffset = PGrnWALPageGetLastOffset(page);
			if (dataOffset > lastOffset)
			{
				PGrnCheckRC(GRN_UNKNOWN_ERROR,
							"[wal][apply][consume][%s(%u)]"
							"[%u/%u] "
							"unconsumed WAL are overwritten: "
							"pgroonga.max_wal_size should be increased or "
							"pgroonga_wal_applier.naptime should be decreased",
							RelationGetRelationName(data->index),
							RelationGetRelid(data->index),
							block,
							dataOffset);
			}
			dataSize = lastOffset - dataOffset;
			if (!msgpack_unpacker_reserve_buffer(&unpacker, dataSize))
			{
				PGrnCheckRC(GRN_NO_MEMORY_AVAILABLE,
							"[wal][apply][consume][%s(%u)]"
							"[%u/%u/%" PGRN_PRIuSIZE "] "
							"failed to allocate buffer to unpack msgpack data",
							RelationGetRelationName(data->index),
							RelationGetRelid(data->index),
							block,
							dataOffset,
							dataSize);
			}
			memcpy(msgpack_unpacker_buffer(&unpacker),
				   PGrnWALPageGetData(page) + dataOffset,
				   dataSize);
			bufferedSize += dataSize;
			UnlockReleaseBuffer(buffer);

			msgpack_unpacker_buffer_consumed(&unpacker, dataSize);
			while (true)
			{
				msgpack_unpack_return unpackResult =
					msgpack_unpacker_next_with_size(
						&unpacker, &unpacked, &parsedSize);
				LocationIndex appliedOffset;

				if (unpackResult < 0)
				{
					PGrnCheckRC(GRN_UNKNOWN_ERROR,
								"[wal][apply][consume][%s(%u)]"
								"[%u/%u/%" PGRN_PRIuSIZE "] "
								"failed to unpack WAL: %d: ",
								RelationGetRelationName(data->index),
								RelationGetRelid(data->index),
								block,
								dataOffset,
								dataSize,
								unpackResult);
				}
				if (unpackResult != MSGPACK_UNPACK_SUCCESS)
				{
					break;
				}

				PGrnWALApplyObject(data, &unpacked.data);
				bufferedSize -= parsedSize;
				appliedOffset = dataOffset + dataSize - bufferedSize;
				PGrnIndexStatusSetWALAppliedPosition(
					data->index, block, appliedOffset);
				nAppliedOperations++;
			}

			if (block == nextBlock)
				break;

			dataOffset = 0;
		}
	}
	PG_CATCH();
	{
		UnlockReleaseBuffer(metaBuffer);
		msgpack_unpacked_destroy(&unpacked);
		msgpack_unpacker_destroy(&unpacker);
		PG_RE_THROW();
	}
	PG_END_TRY();
	UnlockReleaseBuffer(metaBuffer);
	msgpack_unpacked_destroy(&unpacked);
	msgpack_unpacker_destroy(&unpacker);

	return nAppliedOperations;
}
#endif

int64_t
PGrnWALApply(Relation index)
{
	int64_t nAppliedOperations = 0;
#ifdef PGRN_SUPPORT_WAL
	PGrnWALApplyData data;

	if (!PGrnWALEnabled)
		return 0;

	data.index = index;

	if (!PGrnWALApplyNeeded(&data))
		return 0;

	PGrnWALLock(index);
	PGrnIndexStatusGetWALAppliedPosition(
		data.index, &(data.current.block), &(data.current.offset));
	data.sources = NULL;
	nAppliedOperations = PGrnWALApplyConsume(&data);
	PGrnWALUnlock(index);
#endif
	return nAppliedOperations;
}

/**
 * pgroonga_wal_apply(indexName cstring) : bigint
 */
Datum
pgroonga_wal_apply_index(PG_FUNCTION_ARGS)
{
	const char *tag = "[wal][apply][index]";
	int64_t nAppliedOperations = 0;
#ifdef PGRN_SUPPORT_WAL
	Datum indexNameDatum = PG_GETARG_DATUM(0);
	Datum indexOidDatum;
	Oid indexOid;
	Relation index;

	if (!PGrnIsWritable())
	{
		ereport(ERROR,
				(errcode(ERRCODE_E_R_E_MODIFYING_SQL_DATA_NOT_PERMITTED),
				 errmsg("pgroonga: %s "
						"can't apply WAL "
						"while pgroonga.writable is false",
						tag)));
	}

	indexOidDatum = DirectFunctionCall1(regclassin, indexNameDatum);
	indexOid = DatumGetObjectId(indexOidDatum);
	if (!OidIsValid(indexOid))
	{
		PGrnCheckRC(GRN_INVALID_ARGUMENT,
					"%s unknown index name: <%s>",
					tag,
					DatumGetCString(indexNameDatum));
	}

	index = RelationIdGetRelation(indexOid);
	PG_TRY();
	{
		if (!PGrnIndexIsPGroonga(index))
		{
			PGrnCheckRC(GRN_INVALID_ARGUMENT,
						"%s not PGroonga index: <%s>",
						tag,
						DatumGetCString(indexNameDatum));
		}
		if (!PGRN_RELKIND_HAS_PARTITIONS(index->rd_rel->relkind))
		{
			nAppliedOperations = PGrnWALApply(index);
		}
	}
	PG_CATCH();
	{
		RelationClose(index);
		PG_RE_THROW();
	}
	PG_END_TRY();
	RelationClose(index);
#else
	PGrnCheckRC(GRN_FUNCTION_NOT_IMPLEMENTED, "%s not supported", tag);
#endif
	PG_RETURN_INT64(nAppliedOperations);
}

/**
 * pgroonga_wal_apply() : bigint
 */
Datum
pgroonga_wal_apply_all(PG_FUNCTION_ARGS)
{
	const char *tag = "[wal][apply][all]";
	int64_t nAppliedOperations = 0;
#ifdef PGRN_SUPPORT_WAL
	LOCKMODE lock = AccessShareLock;
	Relation indexes;
	TableScanDesc scan;
	HeapTuple indexTuple;

	if (!PGrnIsWritable())
	{
		ereport(ERROR,
				(errcode(ERRCODE_E_R_E_MODIFYING_SQL_DATA_NOT_PERMITTED),
				 errmsg("pgroonga: %s "
						"can't apply WAL "
						"while pgroonga.writable is false",
						tag)));
	}

	indexes = table_open(IndexRelationId, lock);
	scan = table_beginscan_catalog(indexes, 0, NULL);
	while ((indexTuple = heap_getnext(scan, ForwardScanDirection)))
	{
		Form_pg_index indexForm = (Form_pg_index) GETSTRUCT(indexTuple);
		Relation index;

		if (!pgrn_pg_class_ownercheck(indexForm->indexrelid, GetUserId()))
			continue;

		index = RelationIdGetRelation(indexForm->indexrelid);
		if (!PGrnIndexIsPGroonga(index))
		{
			RelationClose(index);
			continue;
		}
		if (PGRN_RELKIND_HAS_PARTITIONS(index->rd_rel->relkind))
		{
			RelationClose(index);
			continue;
		}

		PG_TRY();
		{
			nAppliedOperations += PGrnWALApply(index);
		}
		PG_CATCH();
		{
			RelationClose(index);
			heap_endscan(scan);
			table_close(indexes, lock);
			PG_RE_THROW();
		}
		PG_END_TRY();
		RelationClose(index);
	}
	heap_endscan(scan);
	table_close(indexes, lock);
#else
	PGrnCheckRC(GRN_FUNCTION_NOT_IMPLEMENTED, "%s not supported", tag);
#endif
	PG_RETURN_INT64(nAppliedOperations);
}

#ifdef PGRN_SUPPORT_WAL
static void
PGrnWALGetLastPosition(Relation index,
					   BlockNumber *block,
					   LocationIndex *offset)
{
	BlockNumber nBlocks = RelationGetNumberOfBlocks(index);

	*block = 0;
	*offset = 0;

	if (nBlocks == 0)
		return;

	{
		Buffer metaBuffer = PGrnWALReadLockedBuffer(
			index, PGRN_WAL_META_PAGE_BLOCK_NUMBER, BUFFER_LOCK_SHARE);
		Page metaPage = BufferGetPage(metaBuffer);
		PGrnWALMetaPageSpecial *metaPageSpecial =
			(PGrnWALMetaPageSpecial *) PageGetSpecialPointer(metaPage);
		*block = metaPageSpecial->next;
		if (*block < nBlocks)
		{
			Buffer buffer = PGrnWALReadLockedBuffer(
				index, metaPageSpecial->next, BUFFER_LOCK_SHARE);
			Page page = BufferGetPage(buffer);
			*offset = PGrnWALPageGetLastOffset(page);
			UnlockReleaseBuffer(buffer);
		}
		UnlockReleaseBuffer(metaBuffer);
	}
}
#endif

typedef struct
{
	Relation indexes;
	TableScanDesc scan;
	TupleDesc desc;
} PGrnWALStatusData;

/**
 * pgroonga_wal_status() : SETOF RECORD
 */
Datum
pgroonga_wal_status(PG_FUNCTION_ARGS)
{
	FuncCallContext *context;
#ifdef PGRN_SUPPORT_WAL
	LOCKMODE lock = AccessShareLock;
	PGrnWALStatusData *data;
	HeapTuple indexTuple;

	if (SRF_IS_FIRSTCALL())
	{
		MemoryContext oldContext;
		context = SRF_FIRSTCALL_INIT();
		oldContext = MemoryContextSwitchTo(context->multi_call_memory_ctx);
		PG_TRY();
		{
			const int nAttributes = 8;
			data = palloc(sizeof(PGrnWALStatusData));
			data->indexes = table_open(IndexRelationId, lock);
			data->scan = table_beginscan_catalog(data->indexes, 0, NULL);
			data->desc = CreateTemplateTupleDesc(nAttributes);
			TupleDescInitEntry(data->desc, 1, "name", TEXTOID, -1, 0);
			TupleDescInitEntry(data->desc, 2, "oid", OIDOID, -1, 0);
			TupleDescInitEntry(data->desc, 3, "current_block", INT8OID, -1, 0);
			TupleDescInitEntry(data->desc, 4, "current_offset", INT8OID, -1, 0);
			TupleDescInitEntry(data->desc, 5, "current_size", INT8OID, -1, 0);
			TupleDescInitEntry(data->desc, 6, "last_block", INT8OID, -1, 0);
			TupleDescInitEntry(data->desc, 7, "last_offset", INT8OID, -1, 0);
			TupleDescInitEntry(data->desc, 8, "last_size", INT8OID, -1, 0);
			BlessTupleDesc(data->desc);
			context->user_fctx = data;
		}
		PG_CATCH();
		{
			MemoryContextSwitchTo(oldContext);
			PG_RE_THROW();
		}
		PG_END_TRY();
		MemoryContextSwitchTo(oldContext);
		context->tuple_desc = data->desc;
	}

	context = SRF_PERCALL_SETUP();
	data = context->user_fctx;

	while ((indexTuple = heap_getnext(data->scan, ForwardScanDirection)))
	{
		Form_pg_index indexForm = (Form_pg_index) GETSTRUCT(indexTuple);
		Relation index;
		BlockNumber currentBlock;
		LocationIndex currentOffset;
		Datum *values;
		bool *nulls;
		int i = 0;

		if (!pgrn_pg_class_ownercheck(indexForm->indexrelid, GetUserId()))
			continue;

		index = RelationIdGetRelation(indexForm->indexrelid);
		if (!PGrnIndexIsPGroonga(index))
		{
			RelationClose(index);
			continue;
		}
		if (PGRN_RELKIND_HAS_PARTITIONS(index->rd_rel->relkind))
		{
			RelationClose(index);
			continue;
		}

		values = palloc(sizeof(Datum) * data->desc->natts);
		/* All values are not NULL */
		nulls = palloc0(sizeof(bool) * data->desc->natts);
		values[i++] =
			PointerGetDatum(cstring_to_text(RelationGetRelationName(index)));
		values[i++] = ObjectIdGetDatum(RelationGetRelid(index));
		PGrnIndexStatusGetWALAppliedPosition(
			index, &currentBlock, &currentOffset);
		values[i++] = Int64GetDatum(currentBlock);
		values[i++] = Int64GetDatum(currentOffset);
		values[i++] = Int64GetDatum(currentBlock * BLCKSZ + currentOffset);
		{
			BlockNumber lastBlock = 0;
			LocationIndex lastOffset = 0;
			if (PGrnWALEnabled)
				PGrnWALGetLastPosition(index, &lastBlock, &lastOffset);
			values[i++] = Int64GetDatum(lastBlock);
			values[i++] = Int64GetDatum(lastOffset);
			values[i++] = Int64GetDatum(lastBlock * BLCKSZ + lastOffset);
		}
		RelationClose(index);

		{
			HeapTuple tuple = heap_form_tuple(data->desc, values, nulls);
			SRF_RETURN_NEXT(context, HeapTupleGetDatum(tuple));
		}
	}

	heap_endscan(data->scan);
	table_close(data->indexes, lock);
#else
	if (SRF_IS_FIRSTCALL())
	{
		context = SRF_FIRSTCALL_INIT();
	}
	context = SRF_PERCALL_SETUP();
	{
		const char *tag = "[wal][status]";
		PGrnCheckRC(GRN_FUNCTION_NOT_IMPLEMENTED, "%s not supported", tag);
	}
#endif
	SRF_RETURN_DONE(context);
}

#ifdef PGRN_SUPPORT_WAL
static int64_t
PGrnWALTruncate(Relation index)
{
	int64_t nTruncatedBlocks = 0;
	BlockNumber i;
	BlockNumber nBlocks;
	Buffer processingBuffers[MAX_GENERIC_XLOG_PAGES];
	uint32_t nProcessingBuffers = 0;
	GenericXLogState *state;

	PGrnWALLock(index);
	nBlocks = RelationGetNumberOfBlocks(index);
	if (nBlocks == 0)
	{
		PGrnWALUnlock(index);
		return 0;
	}

	state = GenericXLogStart(index);

	{
		Buffer buffer;
		Page page;
		PGrnWALMetaPageSpecial *pageSpecial;

		buffer = PGrnWALReadLockedBuffer(
			index, PGRN_WAL_META_PAGE_BLOCK_NUMBER, BUFFER_LOCK_EXCLUSIVE);
		processingBuffers[nProcessingBuffers++] = buffer;
		page =
			GenericXLogRegisterBuffer(state, buffer, GENERIC_XLOG_FULL_IMAGE);
		pageSpecial = (PGrnWALMetaPageSpecial *) PageGetSpecialPointer(page);
		pageSpecial->next = PGRN_WAL_META_PAGE_BLOCK_NUMBER + 1;

		nTruncatedBlocks++;
	}

	for (i = PGRN_WAL_META_PAGE_BLOCK_NUMBER + 1; i < nBlocks; i++)
	{
		Buffer buffer;
		Page page;

		if (nProcessingBuffers >= MAX_GENERIC_XLOG_PAGES)
		{
			GenericXLogFinish(state);
			{
				uint32_t j;
				for (j = 0; j < nProcessingBuffers; j++)
				{
					UnlockReleaseBuffer(processingBuffers[j]);
				}
				nProcessingBuffers = 0;
			}
			state = GenericXLogStart(index);
		}
		buffer = PGrnWALReadLockedBuffer(index, i, BUFFER_LOCK_EXCLUSIVE);
		processingBuffers[nProcessingBuffers++] = buffer;
		page =
			GenericXLogRegisterBuffer(state, buffer, GENERIC_XLOG_FULL_IMAGE);
		PageInit(page, BLCKSZ, 0);

		nTruncatedBlocks++;
	}
	GenericXLogFinish(state);

	{
		uint32_t j;
		for (j = 0; j < nProcessingBuffers; j++)
		{
			UnlockReleaseBuffer(processingBuffers[j]);
		}
	}

	PGrnIndexStatusSetWALAppliedPosition(
		index, PGRN_WAL_META_PAGE_BLOCK_NUMBER + 1, 0);

	PGrnWALUnlock(index);

	return nTruncatedBlocks;
}
#endif

/**
 * pgroonga_wal_truncate(indexName cstring) : bigint
 */
Datum
pgroonga_wal_truncate_index(PG_FUNCTION_ARGS)
{
	const char *tag = "[wal][truncate][index]";
	int64_t nTruncatedBlocks = 0;
#ifdef PGRN_SUPPORT_WAL
	Datum indexNameDatum = PG_GETARG_DATUM(0);
	Datum indexOidDatum;
	Oid indexOid;
	Relation index;

	indexOidDatum = DirectFunctionCall1(regclassin, indexNameDatum);
	indexOid = DatumGetObjectId(indexOidDatum);
	if (!OidIsValid(indexOid))
	{
		PGrnCheckRC(GRN_INVALID_ARGUMENT,
					"%s unknown index name: <%s>",
					tag,
					DatumGetCString(indexNameDatum));
	}

	index = RelationIdGetRelation(indexOid);
	PG_TRY();
	{
		if (!PGrnIndexIsPGroonga(index))
		{
			PGrnCheckRC(GRN_INVALID_ARGUMENT,
						"%s not PGroonga index: <%s>",
						tag,
						DatumGetCString(indexNameDatum));
		}
		nTruncatedBlocks = PGrnWALTruncate(index);
	}
	PG_CATCH();
	{
		RelationClose(index);
		PG_RE_THROW();
	}
	PG_END_TRY();
	RelationClose(index);
#else
	PGrnCheckRC(GRN_FUNCTION_NOT_IMPLEMENTED, "%s not supported", tag);
#endif
	PG_RETURN_INT64(nTruncatedBlocks);
}

/**
 * pgroonga_wal_truncate() : bigint
 */
Datum
pgroonga_wal_truncate_all(PG_FUNCTION_ARGS)
{
	int64_t nTruncatedBlocks = 0;
#ifdef PGRN_SUPPORT_WAL
	LOCKMODE lock = AccessShareLock;
	Relation indexes;
	TableScanDesc scan;
	HeapTuple indexTuple;

	indexes = table_open(IndexRelationId, lock);
	scan = table_beginscan_catalog(indexes, 0, NULL);
	while ((indexTuple = heap_getnext(scan, ForwardScanDirection)))
	{
		Form_pg_index indexForm = (Form_pg_index) GETSTRUCT(indexTuple);
		Relation index;

		if (!pgrn_pg_class_ownercheck(indexForm->indexrelid, GetUserId()))
			continue;

		index = RelationIdGetRelation(indexForm->indexrelid);
		if (!PGrnIndexIsPGroonga(index))
		{
			RelationClose(index);
			continue;
		}

		PG_TRY();
		{
			nTruncatedBlocks += PGrnWALTruncate(index);
		}
		PG_CATCH();
		{
			RelationClose(index);
			heap_endscan(scan);
			table_close(indexes, lock);
			PG_RE_THROW();
		}
		PG_END_TRY();
		RelationClose(index);
	}
	heap_endscan(scan);
	table_close(indexes, lock);
#else
	const char *tag = "[wal][truncate][all]";
	PGrnCheckRC(GRN_FUNCTION_NOT_IMPLEMENTED, "%s not supported", tag);
#endif
	PG_RETURN_INT64(nTruncatedBlocks);
}

/**
 * pgroonga_wal_set_applied_position(
 *   indexName cstring,
 *   block bigint,
 *   offset bigint
 * ) : bool
 */
Datum
pgroonga_wal_set_applied_position_index(PG_FUNCTION_ARGS)
{
	const char *tag = "[wal][set-applied-position][index]";
#ifdef PGRN_SUPPORT_WAL
	Datum indexNameDatum = PG_GETARG_DATUM(0);
	BlockNumber block = PG_GETARG_UINT32(1);
	LocationIndex offset = PG_GETARG_UINT32(2);
	Datum indexOidDatum;
	Oid indexOid;
	Relation index;

	if (!PGrnIsWritable())
	{
		ereport(ERROR,
				(errcode(ERRCODE_E_R_E_MODIFYING_SQL_DATA_NOT_PERMITTED),
				 errmsg("pgroonga: %s "
						"can't set WAL applied position "
						"while pgroonga.writable is false",
						tag)));
	}

	indexOidDatum = DirectFunctionCall1(regclassin, indexNameDatum);
	indexOid = DatumGetObjectId(indexOidDatum);
	if (!OidIsValid(indexOid))
	{
		PGrnCheckRC(GRN_INVALID_ARGUMENT,
					"%s unknown index name: <%s>",
					tag,
					DatumGetCString(indexNameDatum));
	}

	index = RelationIdGetRelation(indexOid);
	PG_TRY();
	{
		if (!PGrnIndexIsPGroonga(index))
		{
			PGrnCheckRC(GRN_INVALID_ARGUMENT,
						"%s not PGroonga index: <%s>",
						tag,
						DatumGetCString(indexNameDatum));
		}
		if (PGrnPGIsParentIndex(index))
		{
			PGrnCheckRC(GRN_INVALID_ARGUMENT,
						"%s parent index for declarative partitioning: <%s>",
						tag,
						DatumGetCString(indexNameDatum));
		}
		PGrnWALLock(index);
		PGrnIndexStatusSetWALAppliedPosition(index, block, offset);
		PGrnWALUnlock(index);
	}
	PG_CATCH();
	{
		RelationClose(index);
		PG_RE_THROW();
	}
	PG_END_TRY();
	RelationClose(index);
#else
	PGrnCheckRC(GRN_FUNCTION_NOT_IMPLEMENTED, "%s not supported", tag);
#endif
	PG_RETURN_BOOL(true);
}

/**
 * pgroonga_wal_set_applied_position(indexName cstring) : bool
 */
Datum
pgroonga_wal_set_applied_position_index_last(PG_FUNCTION_ARGS)
{
	const char *tag = "[wal][set-applied-position][index][last]";
#ifdef PGRN_SUPPORT_WAL
	Datum indexNameDatum = PG_GETARG_DATUM(0);
	Datum indexOidDatum;
	Oid indexOid;
	Relation index;

	if (!PGrnIsWritable())
	{
		ereport(ERROR,
				(errcode(ERRCODE_E_R_E_MODIFYING_SQL_DATA_NOT_PERMITTED),
				 errmsg("pgroonga: %s "
						"can't set WAL applied position "
						"while pgroonga.writable is false",
						tag)));
	}

	indexOidDatum = DirectFunctionCall1(regclassin, indexNameDatum);
	indexOid = DatumGetObjectId(indexOidDatum);
	if (!OidIsValid(indexOid))
	{
		PGrnCheckRC(GRN_INVALID_ARGUMENT,
					"%s unknown index name: <%s>",
					tag,
					DatumGetCString(indexNameDatum));
	}

	index = RelationIdGetRelation(indexOid);
	PG_TRY();
	{
		BlockNumber block = 0;
		LocationIndex offset = 0;

		if (!PGrnIndexIsPGroonga(index))
		{
			PGrnCheckRC(GRN_INVALID_ARGUMENT,
						"%s not PGroonga index: <%s>",
						tag,
						DatumGetCString(indexNameDatum));
		}
		if (PGrnPGIsParentIndex(index))
		{
			PGrnCheckRC(GRN_INVALID_ARGUMENT,
						"%s parent index for declarative partitioning: <%s>",
						tag,
						DatumGetCString(indexNameDatum));
		}
		PGrnWALLock(index);
		PGrnWALGetLastPosition(index, &block, &offset);
		PGrnIndexStatusSetWALAppliedPosition(index, block, offset);
		PGrnWALUnlock(index);
	}
	PG_CATCH();
	{
		RelationClose(index);
		PG_RE_THROW();
	}
	PG_END_TRY();
	RelationClose(index);
#else
	PGrnCheckRC(GRN_FUNCTION_NOT_IMPLEMENTED, "%s not supported", tag);
#endif
	PG_RETURN_BOOL(true);
}

/**
 * pgroonga_wal_set_applied_position(block bigint, offset bigint) : bool
 */
Datum
pgroonga_wal_set_applied_position_all(PG_FUNCTION_ARGS)
{
	const char *tag = "[wal][set-applied-position][all]";
#ifdef PGRN_SUPPORT_WAL
	BlockNumber block = PG_GETARG_UINT32(0);
	LocationIndex offset = PG_GETARG_UINT32(1);
	LOCKMODE lock = AccessShareLock;
	Relation indexes;
	TableScanDesc scan;
	HeapTuple indexTuple;

	if (!PGrnIsWritable())
	{
		ereport(ERROR,
				(errcode(ERRCODE_E_R_E_MODIFYING_SQL_DATA_NOT_PERMITTED),
				 errmsg("pgroonga: %s "
						"can't set WAL applied position "
						"while pgroonga.writable is false",
						tag)));
	}

	indexes = table_open(IndexRelationId, lock);
	scan = table_beginscan_catalog(indexes, 0, NULL);
	while ((indexTuple = heap_getnext(scan, ForwardScanDirection)))
	{
		Form_pg_index indexForm = (Form_pg_index) GETSTRUCT(indexTuple);
		Relation index;

		if (!pgrn_pg_class_ownercheck(indexForm->indexrelid, GetUserId()))
			continue;

		index = RelationIdGetRelation(indexForm->indexrelid);
		if (!PGrnIndexIsPGroonga(index))
		{
			RelationClose(index);
			continue;
		}

		PG_TRY();
		{
			PGrnWALLock(index);
			PGrnIndexStatusSetWALAppliedPosition(index, block, offset);
			PGrnWALUnlock(index);
		}
		PG_CATCH();
		{
			RelationClose(index);
			heap_endscan(scan);
			table_close(indexes, lock);
			PG_RE_THROW();
		}
		PG_END_TRY();
		RelationClose(index);
	}
	heap_endscan(scan);
	table_close(indexes, lock);
#else
	PGrnCheckRC(GRN_FUNCTION_NOT_IMPLEMENTED, "%s not supported", tag);
#endif
	PG_RETURN_BOOL(true);
}

/**
 * pgroonga_wal_set_applied_position() : bool
 */
Datum
pgroonga_wal_set_applied_position_all_last(PG_FUNCTION_ARGS)
{
	const char *tag = "[wal][set-applied-position][all][last]";
#ifdef PGRN_SUPPORT_WAL
	LOCKMODE lock = AccessShareLock;
	Relation indexes;
	TableScanDesc scan;
	HeapTuple indexTuple;

	if (!PGrnIsWritable())
	{
		ereport(ERROR,
				(errcode(ERRCODE_E_R_E_MODIFYING_SQL_DATA_NOT_PERMITTED),
				 errmsg("pgroonga: %s "
						"can't set WAL applied position "
						"while pgroonga.writable is false",
						tag)));
	}

	indexes = table_open(IndexRelationId, lock);
	scan = table_beginscan_catalog(indexes, 0, NULL);
	while ((indexTuple = heap_getnext(scan, ForwardScanDirection)))
	{
		Form_pg_index indexForm = (Form_pg_index) GETSTRUCT(indexTuple);
		Relation index;

		if (!pgrn_pg_class_ownercheck(indexForm->indexrelid, GetUserId()))
			continue;

		index = RelationIdGetRelation(indexForm->indexrelid);
		if (!PGrnIndexIsPGroonga(index))
		{
			RelationClose(index);
			continue;
		}
		if (PGrnPGIsParentIndex(index))
		{
			RelationClose(index);
			continue;
		}

		PG_TRY();
		{
			BlockNumber block = 0;
			LocationIndex offset = 0;
			PGrnWALLock(index);
			PGrnWALGetLastPosition(index, &block, &offset);
			PGrnIndexStatusSetWALAppliedPosition(index, block, offset);
			PGrnWALUnlock(index);
		}
		PG_CATCH();
		{
			RelationClose(index);
			heap_endscan(scan);
			table_close(indexes, lock);
			PG_RE_THROW();
		}
		PG_END_TRY();
		RelationClose(index);
	}
	heap_endscan(scan);
	table_close(indexes, lock);
#else
	PGrnCheckRC(GRN_FUNCTION_NOT_IMPLEMENTED, "%s not supported", tag);
#endif
	PG_RETURN_BOOL(true);
}
