#include "pgroonga.h"

#include "pgrn_compatible.h"

#include "pgrn_global.h"
#include "pgrn_xlog.h"

static bool PGrnXLogEnabled = false;

bool
PGrnXLogGetEnabled(void)
{
	return PGrnXLogEnabled;
}

void
PGrnXLogEnable(void)
{
	PGrnXLogEnabled = true;
}

void
PGrnXLogDisable(void)
{
	PGrnXLogEnabled = false;
}

#ifdef PGRN_SUPPORT_XLOG
#	include <access/generic_xlog.h>
#	include <storage/bufmgr.h>
#	include <storage/bufpage.h>
#	include <storage/lmgr.h>
#	include <storage/lockdefs.h>

#	include <msgpack.h>
#endif

#ifdef PGRN_SUPPORT_XLOG
static grn_ctx *ctx = &PGrnContext;
#endif

#ifdef PGRN_SUPPORT_XLOG
typedef struct {
	BlockNumber start;
	BlockNumber current;
	BlockNumber end;
} PGrnMetaPageSpecial;

#define PGRN_PAGE_DATA_SIZE BLCKSZ - SizeOfPageHeaderData - sizeof(OffsetNumber)
typedef struct {
	OffsetNumber current;
	uint8_t data[PGRN_PAGE_DATA_SIZE];
} PGrnPageSpecial;

typedef struct {
	GenericXLogState *state;
	PGrnMetaPageSpecial *metaPageSpecial;
	Buffer buffer;
	Page page;
	PGrnPageSpecial *special;
} PGrnPageWriteData;
#endif

struct PGrnXLogData_
{
	Relation index;
#ifdef PGRN_SUPPORT_XLOG
	GenericXLogState *state;
	struct
	{
		Buffer buffer;
		Page page;
		PGrnMetaPageSpecial *pageSpecial;
	} meta;
	struct
	{
		Buffer buffer;
		Page page;
		PGrnPageSpecial *pageSpecial;
	} current;
	msgpack_packer packer;
#endif
};

#define PGRN_XLOG_META_PAGE_BLOCK_NUMBER 0

#ifdef PGRN_SUPPORT_XLOG
static void
PGrnXLogDataInitMeta(PGrnXLogData *data)
{
	if (RelationGetNumberOfBlocks(data->index) == 0)
	{
		LockRelationForExtension(data->index, ExclusiveLock);
		data->meta.buffer = ReadBuffer(data->index, P_NEW);
		LockBuffer(data->meta.buffer, BUFFER_LOCK_EXCLUSIVE);
		UnlockRelationForExtension(data->index, ExclusiveLock);
	}
	else
	{
		data->meta.buffer = ReadBuffer(data->index,
									   PGRN_XLOG_META_PAGE_BLOCK_NUMBER);
		LockBuffer(data->meta.buffer, BUFFER_LOCK_EXCLUSIVE);
	}

	data->meta.page = GenericXLogRegisterBuffer(data->state,
												data->meta.buffer,
												GENERIC_XLOG_FULL_IMAGE);
	if (PageIsNew(data->meta.page))
	{
		PageInit(data->meta.page, BLCKSZ, sizeof(PGrnMetaPageSpecial));
		data->meta.pageSpecial =
			(PGrnMetaPageSpecial *)PageGetSpecialPointer(data->meta.page);
		data->meta.pageSpecial->start = PGRN_XLOG_META_PAGE_BLOCK_NUMBER + 1;
		data->meta.pageSpecial->current = data->meta.pageSpecial->start;
		data->meta.pageSpecial->end = data->meta.pageSpecial->start;
	}
	else
	{
		data->meta.pageSpecial =
			(PGrnMetaPageSpecial *)PageGetSpecialPointer(data->meta.page);
	}
}

static void
PGrnXLogDataInitCurrent(PGrnXLogData *data)
{
	data->current.buffer = InvalidBuffer;
	data->current.page = NULL;
	data->current.pageSpecial = NULL;
}

static int
PGrnXLogPageWriter(void *userData,
				   const char *buffer,
				   size_t length)
{
	PGrnXLogData *data = userData;
	int written = 0;

	while (written < length)
	{
		if (BufferIsInvalid(data->current.buffer))
		{
			if (RelationGetNumberOfBlocks(data->index) <=
				data->meta.pageSpecial->current)
			{
				LockRelationForExtension(data->index, ExclusiveLock);
				data->current.buffer = ReadBuffer(data->index, P_NEW);
				LockBuffer(data->current.buffer, BUFFER_LOCK_EXCLUSIVE);
				UnlockRelationForExtension(data->index, ExclusiveLock);

				data->meta.pageSpecial->current =
					BufferGetBlockNumber(data->current.buffer);
				data->meta.pageSpecial->end = data->meta.pageSpecial->current;
			}
			else
			{
				data->current.buffer =
					ReadBuffer(data->index, data->meta.pageSpecial->current);
				LockBuffer(data->current.buffer, BUFFER_LOCK_EXCLUSIVE);
			}
		}

		if (!PageIsValid(data->current.page))
		{
			data->current.page =
				GenericXLogRegisterBuffer(data->state,
										  data->current.buffer,
										  GENERIC_XLOG_FULL_IMAGE);
			if (PageIsNew(data->current.page))
			{
				PageInit(data->current.page, BLCKSZ, sizeof(PGrnPageSpecial));
				data->current.pageSpecial =
					(PGrnPageSpecial *)PageGetSpecialPointer(data->current.page);
				data->current.pageSpecial->current = 0;
			}
			else
			{
				data->current.pageSpecial =
					(PGrnPageSpecial *)PageGetSpecialPointer(data->current.page);
			}
		}

		if (data->current.pageSpecial->current + length <= PGRN_PAGE_DATA_SIZE)
		{
			memcpy(data->current.pageSpecial->data +
				   SizeOfPageHeaderData +
				   data->current.pageSpecial->current,
				   buffer,
				   length);
			data->current.pageSpecial->current += length;
			written += length;
		}
		else
		{
			size_t writableSize;

			writableSize =
				PGRN_PAGE_DATA_SIZE - data->current.pageSpecial->current;
			memcpy(data->current.pageSpecial->data +
				   SizeOfPageHeaderData +
				   data->current.pageSpecial->current,
				   buffer,
				   writableSize);
			data->current.pageSpecial->current += writableSize;
			written += writableSize;
			length -= writableSize;
			buffer += writableSize;

			data->current.page = NULL;
			UnlockReleaseBuffer(data->current.buffer);
			data->current.buffer = InvalidBuffer;
			data->meta.pageSpecial->current++;
		}
	}

	return written;
}

static void
PGrnXLogDataInitMessagePack(PGrnXLogData *data)
{
	msgpack_packer_init(&(data->packer), data, PGrnXLogPageWriter);
}
#endif

PGrnXLogData *
PGrnXLogStart(Relation index)
{
#ifdef PGRN_SUPPORT_XLOG
	PGrnXLogData *data;

	if (!PGrnXLogEnabled)
		return NULL;

	data = palloc(sizeof(PGrnXLogData));

	data->index = index;
	data->state = GenericXLogStart(data->index);

	PGrnXLogDataInitMeta(data);
	PGrnXLogDataInitCurrent(data);
	PGrnXLogDataInitMessagePack(data);

	return data;
#else
	return NULL;
#endif
}

void
PGrnXLogFinish(PGrnXLogData *data)
{
#ifdef PGRN_SUPPORT_XLOG
	if (!PGrnXLogEnabled)
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
PGrnXLogAbort(PGrnXLogData *data)
{
#ifdef PGRN_SUPPORT_XLOG
	if (!PGrnXLogEnabled)
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
PGrnXLogInsertStart(PGrnXLogData *data,
					size_t nColumns)
{
#ifdef PGRN_SUPPORT_XLOG
	msgpack_packer *packer;

	if (!PGrnXLogEnabled)
		return;

	packer = &(data->packer);
	msgpack_pack_map(packer, nColumns);
#endif
}

void
PGrnXLogInsertFinish(PGrnXLogData *data)
{
}

void
PGrnXLogInsertColumnStart(PGrnXLogData *data,
						  const char *name)
{
#ifdef PGRN_SUPPORT_XLOG
	msgpack_packer *packer;
	size_t nameSize;

	if (!PGrnXLogEnabled)
		return;

	packer = &(data->packer);

	nameSize = strlen(name);
	msgpack_pack_str(packer, nameSize);
	msgpack_pack_str_body(packer, name, nameSize);
#endif
}

void
PGrnXLogInsertColumnFinish(PGrnXLogData *data)
{
}

void
PGrnXLogInsertColumn(PGrnXLogData *data,
					 const char *name,
					 grn_obj *value)
{
#ifdef PGRN_SUPPORT_XLOG
	msgpack_packer *packer;

	if (!PGrnXLogEnabled)
		return;

	packer = &(data->packer);

	PGrnXLogInsertColumnStart(data, name);

	if (value->header.type != GRN_BULK) {
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("pgroonga: XLog: array value isn't supported yet: <%s>",
						grn_obj_type_to_string(value->header.type))));
	}

	switch (value->header.domain)
	{
	case GRN_DB_BOOL:
		if (GRN_BOOL_VALUE(value))
		{
			msgpack_pack_true(packer);
		}
		else
		{
			msgpack_pack_false(packer);
		}
		break;
	case GRN_DB_INT8:
		msgpack_pack_int8(packer, GRN_INT8_VALUE(value));
		break;
	case GRN_DB_UINT8:
		msgpack_pack_uint8(packer, GRN_UINT8_VALUE(value));
		break;
	case GRN_DB_INT16:
		msgpack_pack_int16(packer, GRN_INT16_VALUE(value));
		break;
	case GRN_DB_UINT16:
		msgpack_pack_uint16(packer, GRN_UINT16_VALUE(value));
		break;
	case GRN_DB_INT32:
		msgpack_pack_int32(packer, GRN_INT32_VALUE(value));
		break;
	case GRN_DB_UINT32:
		msgpack_pack_uint32(packer, GRN_UINT32_VALUE(value));
		break;
	case GRN_DB_INT64:
		msgpack_pack_int64(packer, GRN_INT64_VALUE(value));
		break;
	case GRN_DB_UINT64:
		msgpack_pack_uint64(packer, GRN_UINT64_VALUE(value));
		break;
	case GRN_DB_FLOAT:
		msgpack_pack_double(packer, GRN_FLOAT_VALUE(value));
		break;
	case GRN_DB_TIME:
		msgpack_pack_int64(packer, GRN_TIME_VALUE(value));
		break;
	case GRN_DB_SHORT_TEXT:
	case GRN_DB_TEXT:
	case GRN_DB_LONG_TEXT:
		msgpack_pack_str(packer, GRN_TEXT_LEN(value));
		msgpack_pack_str_body(packer,
							  GRN_TEXT_VALUE(value),
							  GRN_TEXT_LEN(value));
		break;
	default:
		{
			char name[GRN_TABLE_MAX_KEY_SIZE];
			int nameSize;

			nameSize = grn_table_get_key(ctx,
										 grn_ctx_db(ctx),
										 value->header.domain,
										 name,
										 GRN_TABLE_MAX_KEY_SIZE);
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("pgroonga: XLog: unsupported type: <%.*s>",
							nameSize, name)));
		}
		break;
	}

	PGrnXLogInsertColumnFinish(data);
#endif
}
