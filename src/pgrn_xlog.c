#include "pgroonga.h"

#include "pgrn_compatible.h"

#include "pgrn_global.h"
#include "pgrn_groonga.h"
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
static struct PGrnBuffers *buffers = &PGrnBuffers;
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

#define PGRN_XLOG_STATUES_TABLE_NAME "XLogStatuses"
#define PGRN_XLOG_STATUES_TABLE_NAME_SIZE strlen(PGRN_XLOG_STATUES_TABLE_NAME)
#define PGRN_XLOG_STATUES_CURRENT_COLUMN_NAME "current"

static void
PGrnXLogEnsureStatusesTable(void)
{
#ifdef PGRN_SUPPORT_XLOG
	grn_obj *xlogStatuses;

	xlogStatuses = grn_ctx_get(ctx,
							   PGRN_XLOG_STATUES_TABLE_NAME,
							   PGRN_XLOG_STATUES_TABLE_NAME_SIZE);
	if (xlogStatuses)
		return;

	xlogStatuses = PGrnCreateTable(PGRN_XLOG_STATUES_TABLE_NAME,
								   GRN_OBJ_TABLE_HASH_KEY,
								   grn_ctx_at(ctx, GRN_DB_UINT32));
	PGrnCreateColumn(xlogStatuses,
					 PGRN_XLOG_STATUES_CURRENT_COLUMN_NAME,
					 GRN_OBJ_COLUMN_SCALAR,
					 grn_ctx_at(ctx, GRN_DB_UINT64));
#endif
}

#ifdef PGRN_SUPPORT_XLOG
static uint64_t
PGrnXLogPackPosition(BlockNumber block, OffsetNumber offset)
{
	return (((uint64_t)block) << 32) + (uint64_t)offset;
}

static void
PGrnXLogUnpackPosition(uint64_t position,
					   BlockNumber *block,
					   OffsetNumber *offset)
{
	*block = (BlockNumber)(position >> 32);
	*offset = (OffsetNumber)(position & ((1 << 16) - 1));
}

static void
PGrnXLogUpdateStatus(Relation index,
					 BlockNumber block,
					 OffsetNumber offset)
{
	grn_obj *statusesTable;
	grn_obj *currentColumn;
	uint32_t oid;
	grn_id id;
	uint64_t positionRaw;
	grn_obj *position = &(buffers->general);

	PGrnXLogEnsureStatusesTable();

	statusesTable = PGrnLookup(PGRN_XLOG_STATUES_TABLE_NAME, ERROR);
	currentColumn = PGrnLookupColumn(statusesTable,
									 PGRN_XLOG_STATUES_CURRENT_COLUMN_NAME,
									 ERROR);
	oid = RelationGetRelid(index);
	id = grn_table_add(ctx, statusesTable, &oid, sizeof(uint32_t), NULL);
	positionRaw = PGrnXLogPackPosition(block, offset);
	grn_obj_reinit(ctx, position, GRN_DB_UINT64, 0);
	GRN_UINT64_SET(ctx, position, positionRaw);
	grn_obj_set_value(ctx, currentColumn, id, position, GRN_OBJ_SET);
}
#endif

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
			PGrnXLogUpdateStatus(data->index,
								 BufferGetBlockNumber(data->current.buffer),
								 data->current.pageSpecial->current);
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
			PGrnXLogUpdateStatus(data->index,
								 BufferGetBlockNumber(data->current.buffer),
								 data->current.pageSpecial->current);
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

#ifdef PGRN_SUPPORT_XLOG
typedef struct {
	Relation index;
	grn_obj *statusesTable;
	grn_obj *currentColumn;
	grn_id statusID;
	struct {
		BlockNumber block;
		OffsetNumber offset;
	} current;
	grn_obj *sources;
} PGrnXLogApplyData;

static bool
PGrnXLogApplyNeeded(PGrnXLogApplyData *data)
{
	BlockNumber currentBlock;
	OffsetNumber currentOffset;
	BlockNumber nBlocks;

	{
		grn_obj *position = &(buffers->general);
		grn_obj_reinit(ctx, position, GRN_DB_UINT64, 0);
		grn_obj_get_value(ctx, data->currentColumn, data->statusID, position);
		PGrnXLogUnpackPosition(GRN_UINT64_VALUE(position),
							   &currentBlock,
							   &currentOffset);
	}

	nBlocks = RelationGetNumberOfBlocks(data->index);
	if (currentBlock >= nBlocks)
	{
		return false;
	}
	else if (currentBlock == (nBlocks - 1))
	{
		Buffer buffer;
		Page page;
		PGrnPageSpecial *pageSpecial;
		bool needToApply;

		buffer = ReadBuffer(data->index, currentBlock);
		LockBuffer(buffer, BUFFER_LOCK_SHARE);
		page = BufferGetPage(buffer);
		pageSpecial = (PGrnPageSpecial *)PageGetSpecialPointer(page);
		needToApply = (pageSpecial->current > currentOffset);
		UnlockReleaseBuffer(buffer);
		return needToApply;
	} else {
		return true;
	}
}

static void
PGrnXLogApplyObject(PGrnXLogApplyData *data, msgpack_object *object)
{
	grn_id id;
	uint32_t i, nColumns;

	if (object->type != MSGPACK_OBJECT_MAP)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("pgroonga: XLog: apply: record must be map: <%#x>",
						object->type)));
	}

	id = grn_table_add(ctx, data->sources, NULL, 0, NULL);
	nColumns = object->via.map.size;
	for (i = 0; i < nColumns; i++)
	{
		msgpack_object *key;
		msgpack_object *value;
		grn_obj *column;

		key = &(object->via.map.ptr[i].key);
		value = &(object->via.map.ptr[i].val);

		if (key->type != MSGPACK_OBJECT_STR)
		{
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("pgroonga: XLog: apply: key must be map: <%#x>",
							key->type)));
		}

		column = PGrnLookupColumnWithSize(data->sources,
										  key->via.str.ptr,
										  key->via.str.size,
										  ERROR);
		switch (value->type)
		{
		case MSGPACK_OBJECT_BOOLEAN:
			grn_obj_reinit(ctx, &(buffers->general), GRN_DB_BOOL, 0);
			GRN_BOOL_SET(ctx, &(buffers->general), value->via.boolean);
			break;
		case MSGPACK_OBJECT_POSITIVE_INTEGER:
			grn_obj_reinit(ctx, &(buffers->general), GRN_DB_UINT64, 0);
			GRN_UINT64_SET(ctx, &(buffers->general), value->via.u64);
			break;
		case MSGPACK_OBJECT_NEGATIVE_INTEGER:
			grn_obj_reinit(ctx, &(buffers->general), GRN_DB_INT64, 0);
			GRN_INT64_SET(ctx, &(buffers->general), value->via.i64);
			break;
		case MSGPACK_OBJECT_FLOAT:
			grn_obj_reinit(ctx, &(buffers->general), GRN_DB_FLOAT, 0);
			GRN_FLOAT_SET(ctx, &(buffers->general), value->via.f64);
			break;
		case MSGPACK_OBJECT_STR:
			grn_obj_reinit(ctx, &(buffers->general), GRN_DB_TEXT, 0);
			GRN_TEXT_SET(ctx, &(buffers->general),
						 value->via.str.ptr,
						 value->via.str.size);
			break;
/*
		case MSGPACK_OBJECT_ARRAY:
			break;
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
					 errmsg("pgroonga: XLog: apply: unexpected value type: <%#x>",
							value->type)));
			break;
		}
		grn_obj_set_value(ctx, column, id, &(buffers->general), GRN_OBJ_SET);
	}
}

static void
PGrnXLogApplyConsume(PGrnXLogApplyData *data)
{
	BlockNumber i, nBlocks;
	msgpack_unpacker unpacker;
	msgpack_unpacked unpacked;
	BlockNumber lastBlock = data->current.block;
	OffsetNumber lastOffset = data->current.offset;

	data->sources = PGrnLookupSourcesTable(data->index, ERROR);

	msgpack_unpacker_init(&unpacker, PGRN_PAGE_DATA_SIZE);
	msgpack_unpacked_init(&unpacked);
	nBlocks = RelationGetNumberOfBlocks(data->index);
	for (i = data->current.block; i < nBlocks; i++)
	{
		Buffer buffer;
		Page page;
		PGrnPageSpecial *pageSpecial;
		size_t dataSize;

		buffer = ReadBuffer(data->index, i);
		LockBuffer(buffer, BUFFER_LOCK_SHARE);
		page = BufferGetPage(buffer);
		pageSpecial = (PGrnPageSpecial *)PageGetSpecialPointer(page);
		dataSize = pageSpecial->current - data->current.offset;
		msgpack_unpacker_reserve_buffer(&unpacker, dataSize);
		memcpy(msgpack_unpacker_buffer(&unpacker),
			   pageSpecial->data + SizeOfPageHeaderData + data->current.offset,
			   dataSize);
		UnlockReleaseBuffer(buffer);
		data->current.offset = 0;

		msgpack_unpacker_buffer_consumed(&unpacker, dataSize);
		while (msgpack_unpacker_next(&unpacker, &unpacked) ==
			   MSGPACK_UNPACK_SUCCESS)
		{
			PGrnXLogApplyObject(data, &unpacked.data);
		}

		lastBlock = i;
		lastOffset = pageSpecial->current;
	}
	msgpack_unpacked_destroy(&unpacked);
	msgpack_unpacker_destroy(&unpacker);

	PGrnXLogUpdateStatus(data->index, lastBlock, lastOffset);
}
#endif

void
PGrnXLogApply(Relation index)
{
#ifdef PGRN_SUPPORT_XLOG
	PGrnXLogApplyData data;
	uint32_t oid;

	PGrnXLogEnsureStatusesTable();

	data.index = index;
	data.statusesTable = PGrnLookup(PGRN_XLOG_STATUES_TABLE_NAME, ERROR);
	data.currentColumn = PGrnLookupColumn(data.statusesTable,
										  PGRN_XLOG_STATUES_CURRENT_COLUMN_NAME,
										  ERROR);
	oid = RelationGetRelid(index);
	data.statusID = grn_table_add(ctx,
								  data.statusesTable,
								  &oid,
								  sizeof(uint32_t),
								  NULL);
	if (!PGrnXLogApplyNeeded(&data))
		return;

	LockRelation(index, RowExclusiveLock);
	{
		grn_obj *position = &(buffers->general);

		grn_obj_reinit(ctx, position, GRN_DB_UINT64, 0);
		grn_obj_get_value(ctx, data.currentColumn, data.statusID, position);
		PGrnXLogUnpackPosition(GRN_UINT64_VALUE(position),
							   &(data.current.block),
							   &(data.current.offset));
	}
	PGrnXLogApplyConsume(&data);
	UnlockRelation(index, RowExclusiveLock);
#endif
}
