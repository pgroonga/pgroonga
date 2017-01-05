#include "pgroonga.h"

#include "pgrn-compatible.h"

#include "pgrn-global.h"
#include "pgrn-groonga.h"
#include "pgrn-index-status.h"
#include "pgrn-wal.h"

static bool PGrnWALEnabled = false;

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

#ifdef PGRN_SUPPORT_WAL
#	include <access/generic_xlog.h>
#	include <storage/bufmgr.h>
#	include <storage/bufpage.h>
#	include <storage/lmgr.h>
#	include <storage/lockdefs.h>

#	include <msgpack.h>
#endif

#ifdef PGRN_SUPPORT_WAL
static grn_ctx *ctx = &PGrnContext;
static struct PGrnBuffers *buffers = &PGrnBuffers;
#endif

#ifdef PGRN_SUPPORT_WAL
typedef enum {
	PGRN_WAL_ACTION_INSERT,
	PGRN_WAL_ACTION_CREATE_TABLE,
	PGRN_WAL_ACTION_CREATE_COLUMN,
	PGRN_WAL_ACTION_SET_SOURCES
} PGrnWALAction;

#define PGRN_WAL_META_PAGE_SPECIAL_VERSION 1

typedef struct {
	BlockNumber next;
	BlockNumber max; /* TODO */
	uint8_t version;
} PGrnWALMetaPageSpecial;

typedef struct {
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
	msgpack_packer packer;
#endif
};

#ifdef PGRN_SUPPORT_WAL
#	if MSGPACK_VERSION_MAJOR == 0
#		define MSGPACK_OBJECT_FLOAT MSGPACK_OBJECT_DOUBLE
#		define MSGPACK_OBJECT_STR   MSGPACK_OBJECT_RAW
#		define MSGPACK_OBJECT_BIN   MSGPACK_OBJECT_RAW
#	endif

#	if MSGPACK_VERSION_MAJOR == 0
#		define MSGPACK_OBJECT_VIA_FLOAT(object) ((object).via.dec)
#		define MSGPACK_OBJECT_VIA_STR(object)   ((object).via.raw)
#		define MSGPACK_OBJECT_VIA_BIN(object)   ((object).via.raw)
#	else
#		define MSGPACK_OBJECT_VIA_FLOAT(object) ((object).via.f64)
#		define MSGPACK_OBJECT_VIA_STR(object)   ((object).via.str)
#		define MSGPACK_OBJECT_VIA_BIN(object)   ((object).via.bin)
#	endif
#endif

#ifdef PGRN_SUPPORT_WAL
#	if MSGPACK_VERSION_MAJOR == 0
#		define msgpack_pack_str(packer, size) \
	msgpack_pack_raw((packer), (size))
#		define msgpack_pack_str_body(packer, data, size) \
	msgpack_pack_raw_body((packer), (data), (size))
#		define msgpack_pack_bin(packer, size) \
	msgpack_pack_raw((packer), (size))
#		define msgpack_pack_bin_body(packer, data, size) \
	msgpack_pack_raw_body((packer), (data), (size))
#	endif

#ifdef PGRN_SUPPORT_WAL
#	if MSGPACK_VERSION_MAJOR == 0
#		define MSGPACK_UNPACKER_NEXT(unpacker, unpacked) \
	msgpack_unpacker_next((unpacker), (unpacked))
#	else
#		define MSGPACK_UNPACKER_NEXT(unpacker, unpacked) \
	msgpack_unpacker_next((unpacker), (unpacked)) == MSGPACK_UNPACK_SUCCESS
#	endif
#endif

#ifdef PGRN_SUPPORT_WAL
#	if MSGPACK_VERSION_MAJOR == 0
#		define MSGPACK_PACKER_WRITE_LENGTH_TYPE unsigned int
#	else
#		define MSGPACK_PACKER_WRITE_LENGTH_TYPE size_t
#	endif
#endif

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
		char name[GRN_TABLE_MAX_KEY_SIZE];
		int nameSize;
		nameSize = grn_obj_name(ctx, object, name, GRN_TABLE_MAX_KEY_SIZE);
		msgpack_pack_str(packer, nameSize);
		msgpack_pack_str_body(packer, name, nameSize);
	}
	else
	{
		msgpack_pack_nil(packer);
	}
}
#endif

#define PGRN_WAL_META_PAGE_BLOCK_NUMBER 0

#ifdef PGRN_SUPPORT_WAL
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
		LockRelationForExtension(data->index, ExclusiveLock);
		data->meta.buffer = ReadBuffer(data->index, P_NEW);
		LockBuffer(data->meta.buffer, BUFFER_LOCK_EXCLUSIVE);
		UnlockRelationForExtension(data->index, ExclusiveLock);

		data->meta.page = GenericXLogRegisterBuffer(data->state,
													data->meta.buffer,
													GENERIC_XLOG_FULL_IMAGE);
		PageInit(data->meta.page, BLCKSZ, sizeof(PGrnWALMetaPageSpecial));
		data->meta.pageSpecial =
			(PGrnWALMetaPageSpecial *)PageGetSpecialPointer(data->meta.page);
		data->meta.pageSpecial->next = PGRN_WAL_META_PAGE_BLOCK_NUMBER + 1;
		data->meta.pageSpecial->max = 0;
		data->meta.pageSpecial->version = PGRN_WAL_META_PAGE_SPECIAL_VERSION;
	}
	else
	{
		data->meta.buffer = ReadBuffer(data->index,
									   PGRN_WAL_META_PAGE_BLOCK_NUMBER);
		LockBuffer(data->meta.buffer, BUFFER_LOCK_EXCLUSIVE);

		data->meta.page = GenericXLogRegisterBuffer(data->state,
													data->meta.buffer,
													0);
		data->meta.pageSpecial =
			(PGrnWALMetaPageSpecial *)PageGetSpecialPointer(data->meta.page);
	}
}

static void
PGrnWALDataInitCurrent(PGrnWALData *data)
{
	data->current.buffer = InvalidBuffer;
	data->current.page = NULL;
}

static void
PGrnWALDataRestart(PGrnWALData *data)
{
	GenericXLogFinish(data->state);

	if (data->current.buffer)
	{
		UnlockReleaseBuffer(data->current.buffer);
	}
	UnlockReleaseBuffer(data->meta.buffer);

	data->state = GenericXLogStart(data->index);
	PGrnWALDataInitNUsedPages(data);
	PGrnWALDataInitMeta(data);
	PGrnWALDataInitCurrent(data);
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

	pageHeader = (PageHeader)page;
	return pageHeader->pd_upper - pageHeader->pd_lower;
}

static LocationIndex
PGrnWALPageGetLastOffset(Page page)
{
	PageHeader pageHeader;

	pageHeader = (PageHeader)page;
	return pageHeader->pd_lower - SizeOfPageHeaderData;
}

static void
PGrnWALPageAppend(Page page, const char *data, size_t dataSize)
{
	PageHeader pageHeader;

	pageHeader = (PageHeader)page;
	memcpy(PGrnWALPageGetData(page) + PGrnWALPageGetLastOffset(page),
		   data,
		   dataSize);
	pageHeader->pd_lower += dataSize;
}

static void
PGrnWALPageWriterEnsureCurrent(PGrnWALData *data)
{
	PGrnWALMetaPageSpecial *meta;

	if (!BufferIsInvalid(data->current.buffer))
		return;

	if (RelationGetNumberOfBlocks(data->index) <= data->meta.pageSpecial->next &&
		data->nUsedPages == MAX_GENERIC_XLOG_PAGES)
	{
		PGrnWALDataRestart(data);
	}

	meta = data->meta.pageSpecial;
	if (RelationGetNumberOfBlocks(data->index) <= meta->next)
	{
		LockRelationForExtension(data->index, ExclusiveLock);
		data->current.buffer = ReadBuffer(data->index, P_NEW);
		LockBuffer(data->current.buffer, BUFFER_LOCK_EXCLUSIVE);
		UnlockRelationForExtension(data->index, ExclusiveLock);

		meta->next = BufferGetBlockNumber(data->current.buffer);

		data->current.page =
			GenericXLogRegisterBuffer(data->state,
									  data->current.buffer,
									  GENERIC_XLOG_FULL_IMAGE);
		PageInit(data->current.page, BLCKSZ, 0);
	}
	else
	{
		data->current.buffer = ReadBuffer(data->index, meta->next);
		LockBuffer(data->current.buffer, BUFFER_LOCK_EXCLUSIVE);

		data->current.page =
			GenericXLogRegisterBuffer(data->state,
									  data->current.buffer,
									  0);
	}

	data->nUsedPages++;
}

static int
PGrnWALPageWriter(void *userData,
				  const char *buffer,
				  MSGPACK_PACKER_WRITE_LENGTH_TYPE length)
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
			PGrnIndexStatusSetWALAppliedPosition(
				data->index,
				BufferGetBlockNumber(data->current.buffer),
				PGrnWALPageGetLastOffset(data->current.page));
			written += rest;
		}
		else
		{
			PGrnWALPageAppend(data->current.page, buffer, freeSize);
			written += freeSize;
			rest -= freeSize;
			buffer += freeSize;

			data->current.page = NULL;
			UnlockReleaseBuffer(data->current.buffer);
			data->current.buffer = InvalidBuffer;
			data->meta.pageSpecial->next++;
		}
	}

	return written;
}

static void
PGrnWALDataInitMessagePack(PGrnWALData *data)
{
	msgpack_packer_init(&(data->packer), data, PGrnWALPageWriter);
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

	data = palloc(sizeof(PGrnWALData));

	data->index = index;
	data->state = GenericXLogStart(data->index);

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

	GenericXLogFinish(data->state);

	if (data->current.buffer)
	{
		UnlockReleaseBuffer(data->current.buffer);
	}
	UnlockReleaseBuffer(data->meta.buffer);

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

	if (data->current.buffer)
	{
		UnlockReleaseBuffer(data->current.buffer);
	}
	UnlockReleaseBuffer(data->meta.buffer);

	pfree(data);
#endif
}

void
PGrnWALInsertStart(PGrnWALData *data,
				   grn_obj *table,
				   size_t nColumns)
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

		tableNameSize = grn_obj_name(ctx,
									 table,
									 tableName,
									 GRN_TABLE_MAX_KEY_SIZE);
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
PGrnWALInsertColumnStart(PGrnWALData *data,
						 const char *name,
						 size_t nameSize)
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
	msgpack_packer *packer;

	packer = &(data->packer);

	switch (domain)
	{
	case GRN_DB_BOOL:
		if (*((grn_bool *)value))
		{
			msgpack_pack_true(packer);
		}
		else
		{
			msgpack_pack_false(packer);
		}
		break;
	case GRN_DB_INT8:
		msgpack_pack_int8(packer, *((int8_t *)(value)));
		break;
	case GRN_DB_UINT8:
		msgpack_pack_uint8(packer, *((uint8_t *)(value)));
		break;
	case GRN_DB_INT16:
		msgpack_pack_int16(packer, *((int16_t *)(value)));
		break;
	case GRN_DB_UINT16:
		msgpack_pack_uint16(packer, *((uint16_t *)(value)));
		break;
	case GRN_DB_INT32:
		msgpack_pack_int32(packer, *((int32_t *)(value)));
		break;
	case GRN_DB_UINT32:
		msgpack_pack_uint32(packer, *((uint32_t *)(value)));
		break;
	case GRN_DB_INT64:
		msgpack_pack_int64(packer, *((int64_t *)(value)));
		break;
	case GRN_DB_UINT64:
		msgpack_pack_uint64(packer, *((uint64_t *)(value)));
		break;
	case GRN_DB_FLOAT:
		msgpack_pack_double(packer, *((float_t *)(value)));
		break;
	case GRN_DB_TIME:
		msgpack_pack_int64(packer, *((int64_t *)(value)));
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

			domainNameSize = grn_table_get_key(ctx,
											   grn_ctx_db(ctx),
											   domain,
											   domainName,
											   GRN_TABLE_MAX_KEY_SIZE);
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("pgroonga: WAL: insert: unsupported type: "
							"<%.*s>: <%.*s>",
							(int)nameSize, name,
							domainNameSize, domainName)));
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

		elementSize = grn_vector_get_element(ctx,
											 value,
											 i,
											 &element,
											 NULL,
											 &domain);
		PGrnWALInsertColumnValueRaw(data,
									name,
									nameSize,
									domain,
									element,
									elementSize);
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
		PGrnWALInsertColumnValueRaw(data,
									name,
									nameSize,
									domain,
									element,
									elementSize);
	}
}
#endif

void
PGrnWALInsertColumn(PGrnWALData *data,
					grn_obj *column,
					grn_obj *value)
{
#ifdef PGRN_SUPPORT_WAL
	char name[GRN_TABLE_MAX_KEY_SIZE];
	int nameSize;

	if (!data)
		return;

	nameSize = grn_column_name(ctx, column, name, GRN_TABLE_MAX_KEY_SIZE);

	if (!data)
		return;

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
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("pgroonga: WAL: not bulk value isn't supported yet: "
						"<%.*s>: <%s>",
						(int)nameSize, name,
						grn_obj_type_to_string(value->header.type))));
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

	PGrnWALInsertColumnStart(data,
							 GRN_COLUMN_NAME_KEY,
							 GRN_COLUMN_NAME_KEY_LEN);
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

	PGrnWALInsertKeyRaw(data,
						GRN_BULK_HEAD(key),
						GRN_BULK_VSIZE(key));
#endif
}

void
PGrnWALCreateTable(Relation index,
				   const char *name,
				   size_t nameSize,
				   grn_table_flags flags,
				   grn_obj *type,
				   grn_obj *tokenizer,
				   grn_obj *normalizer)
{
#ifdef PGRN_SUPPORT_WAL
	PGrnWALData *data;
	msgpack_packer *packer;
	size_t nElements = 6;

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

	msgpack_pack_cstr(packer, "normalizer");
	msgpack_pack_grn_obj(packer, normalizer);

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
PGrnWALSetSourceIDs(Relation index,
					grn_obj *column,
					grn_obj *sourceIDs)
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

#ifdef PGRN_SUPPORT_WAL
typedef struct {
	Relation index;
	struct {
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

	if (!PGrnWALEnabled)
		return false;

	PGrnIndexStatusGetWALAppliedPosition(data->index,
										 &currentBlock,
										 &currentOffset);

	nBlocks = RelationGetNumberOfBlocks(data->index);
	if (currentBlock >= nBlocks)
	{
		return false;
	}
	else if (currentBlock == (nBlocks - 1))
	{
		Buffer buffer;
		Page page;
		bool needToApply;

		buffer = ReadBuffer(data->index, currentBlock);
		LockBuffer(buffer, BUFFER_LOCK_SHARE);
		page = BufferGetPage(buffer);
		needToApply = (PGrnWALPageGetLastOffset(page) > currentOffset);
		UnlockReleaseBuffer(buffer);
		return needToApply;
	}
	else
	{
		return true;
	}
}

static bool
PGrnWALApplyKeyEqual(const char *context,
					 msgpack_object *key,
					 const char *name)
{
	size_t nameSize;

	if (key->type != MSGPACK_OBJECT_STR)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("pgroonga: WAL: apply: %s%s"
						"key must be string: <%#x>",
						context ? context : "",
						context ? ": " : "",
						key->type)));
	}

	nameSize = strlen(name);
	if (MSGPACK_OBJECT_VIA_STR(*key).size != nameSize)
		return false;
	if (memcmp(MSGPACK_OBJECT_VIA_STR(*key).ptr, name, nameSize) != 0)
		return false;

	return true;
}

static uint64_t
PGrnWALApplyValueGetPositiveInteger(const char *context,
									msgpack_object_kv *kv)
{
	if (kv->val.type != MSGPACK_OBJECT_POSITIVE_INTEGER)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("pgroonga: WAL: apply: %s%s"
						"%.*s value must be positive integer: "
						"<%#x>",
						context ? context : "",
						context ? ": " : "",
						MSGPACK_OBJECT_VIA_STR(kv->key).size,
						MSGPACK_OBJECT_VIA_STR(kv->key).ptr,
						kv->val.type)));
	}

	return kv->val.via.u64;
}

static void
PGrnWALApplyValueGetString(const char *context,
						   msgpack_object_kv *kv,
						   const char **string,
						   size_t *stringSize)
{
	if (kv->val.type != MSGPACK_OBJECT_STR)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("pgroonga: WAL: apply: %s%s"
						"%.*s value must be string: "
						"<%#x>",
						context ? context : "",
						context ? ": " : "",
						MSGPACK_OBJECT_VIA_STR(kv->key).size,
						MSGPACK_OBJECT_VIA_STR(kv->key).ptr,
						kv->val.type)));
	}

	*string = MSGPACK_OBJECT_VIA_STR(kv->val).ptr;
	*stringSize = MSGPACK_OBJECT_VIA_STR(kv->val).size;
}

static grn_obj *
PGrnWALApplyValueGetGroongaObject(const char *context,
								  msgpack_object_kv *kv)
{
	grn_obj *object = NULL;

	switch (kv->val.type)
	{
	case MSGPACK_OBJECT_NIL:
		object = NULL;
		break;
	case MSGPACK_OBJECT_STR:
		object = PGrnLookupWithSize(MSGPACK_OBJECT_VIA_STR(kv->val).ptr,
									MSGPACK_OBJECT_VIA_STR(kv->val).size,
									ERROR);
		break;
	default:
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("pgroonga: WAL: apply: %s%s"
						"%.*s value must be nil or string: "
						"<%#x>",
						context ? context : "",
						context ? ": " : "",
						MSGPACK_OBJECT_VIA_STR(kv->key).size,
						MSGPACK_OBJECT_VIA_STR(kv->key).ptr,
						kv->val.type)));
		break;
	}

	return object;
}

static void
PGrnWALApplyValueGetGroongaObjectIDs(const char *context,
									 msgpack_object_kv *kv,
									 grn_obj *ids)
{
	msgpack_object_array *array;
	uint32_t i;

	if (kv->val.type != MSGPACK_OBJECT_ARRAY)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("pgroonga: WAL: apply: %s%s"
						"%.*s value must be array: "
						"<%#x>",
						context ? context : "",
						context ? ": " : "",
						MSGPACK_OBJECT_VIA_STR(kv->key).size,
						MSGPACK_OBJECT_VIA_STR(kv->key).ptr,
						kv->val.type)));
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
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("pgroonga: WAL: apply: %s%s"
							"%.*s value must be array of string: "
							"[%u]=<%#x>",
							context ? context : "",
							context ? ": " : "",
							MSGPACK_OBJECT_VIA_STR(kv->key).size,
							MSGPACK_OBJECT_VIA_STR(kv->key).ptr,
							i,
							element->type)));
		}

		object = PGrnLookupWithSize(MSGPACK_OBJECT_VIA_STR(*element).ptr,
									MSGPACK_OBJECT_VIA_STR(*element).size,
									ERROR);
		objectID = grn_obj_id(ctx, object);
		GRN_RECORD_PUT(ctx, ids, objectID);
	}
}

static void
PGrnWALApplyInsertArray(PGrnWALApplyData *data,
						msgpack_object_array *array,
						grn_obj *value,
						grn_id range_id)
{
	const char *context = "insert: array";
	uint32_t i;

	grn_obj_reinit(ctx, value, range_id, GRN_OBJ_VECTOR);
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
#define ELEMENT_VALUE											\
			(element->type == MSGPACK_OBJECT_POSITIVE_INTEGER ? \
			 element->via.u64 :									\
			 element->via.i64)
			switch (range_id)
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
			}
			break;
#undef ELEMENT_VALUE
		case MSGPACK_OBJECT_FLOAT:
			GRN_FLOAT_PUT(ctx, value, MSGPACK_OBJECT_VIA_FLOAT(*element));
			break;
		case MSGPACK_OBJECT_STR:
			grn_vector_add_element(ctx,
								   value,
								   MSGPACK_OBJECT_VIA_STR(*element).ptr,
								   MSGPACK_OBJECT_VIA_STR(*element).size,
								   0,
								   range_id);
			break;
		default:
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("pgroonga: WAL: apply: %s: "
							"unexpected element type: <%#x>",
							context,
							element->type)));
			break;
		}
	}
}

static void
PGrnWALApplyInsert(PGrnWALApplyData *data,
				   msgpack_object_map *map,
				   uint32_t currentElement)
{
	const char *context = "insert";
	grn_obj *table = NULL;
	const char *key = NULL;
	size_t keySize = 0;
	grn_id id;
	uint32_t i;

	if (currentElement < map->size)
	{
		msgpack_object_kv *kv;

		kv = &(map->ptr[currentElement]);
		if (PGrnWALApplyKeyEqual(context, &(kv->key), "_table"))
		{
			table = PGrnWALApplyValueGetGroongaObject(context, kv);
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
		if (PGrnWALApplyKeyEqual(context, &(kv->key), GRN_COLUMN_NAME_KEY))
		{
			if (kv->val.type != MSGPACK_OBJECT_BIN)
			{
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						 errmsg("pgroonga: WAL: apply: %s: "
								"%s value must be binary: "
								"<%#x>",
								context,
								GRN_COLUMN_NAME_KEY,
								kv->val.type)));
			}
			key = MSGPACK_OBJECT_VIA_BIN(kv->val).ptr;
			keySize = MSGPACK_OBJECT_VIA_BIN(kv->val).size;
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
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("pgroonga: WAL: apply: %s: "
							"key must be map: <%#x>",
							context,
							key->type)));
		}

		column = PGrnLookupColumnWithSize(table,
										  MSGPACK_OBJECT_VIA_STR(*key).ptr,
										  MSGPACK_OBJECT_VIA_STR(*key).size,
										  ERROR);
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
			GRN_FLOAT_SET(ctx, walValue, MSGPACK_OBJECT_VIA_FLOAT(*value));
			break;
		case MSGPACK_OBJECT_STR:
			grn_obj_reinit(ctx, walValue, GRN_DB_TEXT, 0);
			GRN_TEXT_SET(ctx, walValue,
						 MSGPACK_OBJECT_VIA_STR(*value).ptr,
						 MSGPACK_OBJECT_VIA_STR(*value).size);
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
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("pgroonga: WAL: apply: %s: "
							"unexpected value type: <%#x>",
							context,
							value->type)));
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
	const char *context = "create table";
	const char *name = NULL;
	size_t nameSize = 0;
	grn_table_flags flags = 0;
	grn_obj *type = NULL;
	grn_obj *tokenizer = NULL;
	grn_obj *normalizer = NULL;
	uint32_t i;

	for (i = currentElement; i < map->size; i++)
	{
		msgpack_object_kv *kv;

		kv = &(map->ptr[i]);
		if (PGrnWALApplyKeyEqual(context, &(kv->key), "name"))
		{
			PGrnWALApplyValueGetString(context, kv, &name, &nameSize);
		}
		else if (PGrnWALApplyKeyEqual(context, &(kv->key), "flags"))
		{
			flags = PGrnWALApplyValueGetPositiveInteger(context, kv);
		}
		else if (PGrnWALApplyKeyEqual(context, &(kv->key), "type"))
		{
			type = PGrnWALApplyValueGetGroongaObject(context, kv);
		}
		else if (PGrnWALApplyKeyEqual(context, &(kv->key), "tokenizer"))
		{
			tokenizer = PGrnWALApplyValueGetGroongaObject(context, kv);
		}
		else if (PGrnWALApplyKeyEqual(context, &(kv->key), "normalizer"))
		{
			normalizer = PGrnWALApplyValueGetGroongaObject(context, kv);
		}
	}

	PGrnCreateTableWithSize(NULL,
							name,
							nameSize,
							flags,
							type,
							tokenizer,
							normalizer);
}

static void
PGrnWALApplyCreateColumn(PGrnWALApplyData *data,
						 msgpack_object_map *map,
						 uint32_t currentElement)
{
	const char *context = "create column";
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
		if (PGrnWALApplyKeyEqual(context, &(kv->key), "table"))
		{
			table = PGrnWALApplyValueGetGroongaObject(context, kv);
		}
		else if (PGrnWALApplyKeyEqual(context, &(kv->key), "name"))
		{
			PGrnWALApplyValueGetString(context, kv, &name, &nameSize);
		}
		else if (PGrnWALApplyKeyEqual(context, &(kv->key), "flags"))
		{
			flags = PGrnWALApplyValueGetPositiveInteger(context, kv);
		}
		else if (PGrnWALApplyKeyEqual(context, &(kv->key), "type"))
		{
			type = PGrnWALApplyValueGetGroongaObject(context, kv);
		}
	}

	PGrnCreateColumnWithSize(NULL, table, name, nameSize, flags, type);
}

static void
PGrnWALApplySetSources(PGrnWALApplyData *data,
					   msgpack_object_map *map,
					   uint32_t currentElement)
{
	const char *context = "set sources";
	grn_obj *column = NULL;
	grn_obj *sourceIDs = &(buffers->sourceIDs);
	uint32_t i;

	GRN_BULK_REWIND(sourceIDs);
	for (i = currentElement; i < map->size; i++)
	{
		msgpack_object_kv *kv;

		kv = &(map->ptr[i]);
		if (PGrnWALApplyKeyEqual(context, &(kv->key), "column"))
		{
			column = PGrnWALApplyValueGetGroongaObject(context, kv);
		}
		else if (PGrnWALApplyKeyEqual(context, &(kv->key), "sources"))
		{
			PGrnWALApplyValueGetGroongaObjectIDs(context, kv, sourceIDs);
		}
	}

	grn_obj_set_info(ctx, column, GRN_INFO_SOURCE, sourceIDs);
}

static void
PGrnWALApplyObject(PGrnWALApplyData *data, msgpack_object *object)
{
	const char *context = NULL;
	msgpack_object_map *map;
	uint32_t currentElement = 0;
	PGrnWALAction action = PGRN_WAL_ACTION_INSERT;

	if (object->type != MSGPACK_OBJECT_MAP)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("pgroonga: WAL: apply: record must be map: <%#x>",
						object->type)));
	}

	map = &(object->via.map);

	if (currentElement < map->size)
	{
		msgpack_object_kv *kv;

		kv = &(object->via.map.ptr[currentElement]);
		if (PGrnWALApplyKeyEqual(context, &(kv->key), "_action"))
		{
			action = PGrnWALApplyValueGetPositiveInteger(context, kv);
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
	default:
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("pgroonga: WAL: apply: unexpected action: <%d>",
						action)));
		break;
	}
}

static void
PGrnWALApplyConsume(PGrnWALApplyData *data)
{
	BlockNumber i;
	BlockNumber startBlock;
	LocationIndex dataOffset;
	BlockNumber nBlocks;
	msgpack_unpacker unpacker;
	msgpack_unpacked unpacked;

	msgpack_unpacker_init(&unpacker, MSGPACK_UNPACKER_INIT_BUFFER_SIZE);
	msgpack_unpacked_init(&unpacked);
	startBlock = data->current.block;
	dataOffset = data->current.offset;
	if (startBlock == PGRN_WAL_META_PAGE_BLOCK_NUMBER)
		startBlock++;
	nBlocks = RelationGetNumberOfBlocks(data->index);
	for (i = startBlock; i < nBlocks; i++)
	{
		Buffer buffer;
		Page page;
		size_t dataSize;

		buffer = ReadBuffer(data->index, i);
		LockBuffer(buffer, BUFFER_LOCK_SHARE);
		page = BufferGetPage(buffer);
		dataSize = PGrnWALPageGetLastOffset(page) - dataOffset;
		msgpack_unpacker_reserve_buffer(&unpacker, dataSize);
		memcpy(msgpack_unpacker_buffer(&unpacker),
			   PGrnWALPageGetData(page) + dataOffset,
			   dataSize);
		UnlockReleaseBuffer(buffer);

		msgpack_unpacker_buffer_consumed(&unpacker, dataSize);
		while (MSGPACK_UNPACKER_NEXT(&unpacker, &unpacked))
		{
			LocationIndex appliedOffset;

			PGrnWALApplyObject(data, &unpacked.data);
			appliedOffset =
				dataOffset +
				dataSize - (unpacker.used - unpacker.off);
			PGrnIndexStatusSetWALAppliedPosition(data->index,
												 i,
												 appliedOffset);
		}
		dataOffset = 0;
	}
	msgpack_unpacked_destroy(&unpacked);
	msgpack_unpacker_destroy(&unpacker);
}
#endif

void
PGrnWALApply(Relation index)
{
#ifdef PGRN_SUPPORT_WAL
	PGrnWALApplyData data;

	data.index = index;
	if (!PGrnWALApplyNeeded(&data))
		return;

	LockRelation(index, RowExclusiveLock);
	PGrnIndexStatusGetWALAppliedPosition(data.index,
										 &(data.current.block),
										 &(data.current.offset));
	data.sources = NULL;
	PGrnWALApplyConsume(&data);
	UnlockRelation(index, RowExclusiveLock);
#endif
}
