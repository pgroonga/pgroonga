#include "pgroonga.h"

#include "pgrn_global.h"
#include "pgrn_groonga.h"
#include "pgrn_index_status.h"
#include "pgrn_wal.h"

#include <miscadmin.h>

static grn_ctx *ctx = &PGrnContext;
static struct PGrnBuffers *buffers = &PGrnBuffers;

#define TABLE_NAME "IndexStatuses"
#define TABLE_NAME_SIZE (sizeof(TABLE_NAME) - 1)
#define MAX_RECORD_SIZE_COLUMN_NAME "max_record_size"
#define WAL_APPLIED_POSITION_COLUMN_NAME "wal_applied_position"

void
PGrnInitializeIndexStatus(void)
{
	grn_obj *table;

	table = grn_ctx_get(ctx,
						TABLE_NAME,
						TABLE_NAME_SIZE);
	if (!table)
	{
		table = PGrnCreateTableWithSize(NULL,
										TABLE_NAME,
										TABLE_NAME_SIZE,
										GRN_OBJ_TABLE_HASH_KEY,
										grn_ctx_at(ctx, GRN_DB_UINT32),
										NULL,
										NULL);
	}

	if (!grn_ctx_get(ctx, TABLE_NAME "." MAX_RECORD_SIZE_COLUMN_NAME, -1))
	{
		PGrnCreateColumn(NULL,
						 table,
						 MAX_RECORD_SIZE_COLUMN_NAME,
						 GRN_OBJ_COLUMN_SCALAR,
						 grn_ctx_at(ctx, GRN_DB_UINT32));
	}

	if (!grn_ctx_get(ctx, TABLE_NAME "." WAL_APPLIED_POSITION_COLUMN_NAME, -1))
	{
		PGrnCreateColumn(NULL,
						 table,
						 WAL_APPLIED_POSITION_COLUMN_NAME,
						 GRN_OBJ_COLUMN_SCALAR,
						 grn_ctx_at(ctx, GRN_DB_UINT64));
	}
}

static grn_id
PGrnIndexStatusGetRecordIDWithWAL(Relation index,
								  PGrnWALData **walData,
								  size_t nColumns)
{
	grn_obj *table;
	const void *key;
	size_t keySize;
	grn_id id;

	table = PGrnLookupWithSize(TABLE_NAME, TABLE_NAME_SIZE, ERROR);
	key = &(index->rd_node.relNode);
	keySize = sizeof(uint32_t);
	id = grn_table_add(ctx, table, key, keySize, NULL);
	if (id != GRN_ID_NIL && walData)
	{
		*walData = PGrnWALStart(index);
		PGrnWALInsertStart(*walData, table, nColumns);
		PGrnWALInsertKeyRaw(*walData, key, keySize);
	}
	return id;
}

static grn_id
PGrnIndexStatusGetRecordID(Relation index)
{
	return PGrnIndexStatusGetRecordIDWithWAL(index, NULL, 0);
}

uint32_t
PGrnIndexStatusGetMaxRecordSize(Relation index)
{
	grn_id id;
	grn_obj *column;
	grn_obj *maxRecordSize = &(buffers->maxRecordSize);

	id = PGrnIndexStatusGetRecordID(index);
	column = PGrnLookup(TABLE_NAME "." MAX_RECORD_SIZE_COLUMN_NAME,
						ERROR);
	GRN_BULK_REWIND(maxRecordSize);
	grn_obj_get_value(ctx, column, id, maxRecordSize);
	return GRN_UINT32_VALUE(maxRecordSize);
}

void
PGrnIndexStatusSetMaxRecordSize(Relation index, uint32_t size)
{
	grn_id id;
	grn_obj *column;
	grn_obj *maxRecordSize = &(buffers->maxRecordSize);
	PGrnWALData *walData = NULL;
	size_t nColumns = 2;

	id = PGrnIndexStatusGetRecordIDWithWAL(index, &walData, nColumns);
	column = PGrnLookup(TABLE_NAME "." MAX_RECORD_SIZE_COLUMN_NAME,
						ERROR);
	GRN_UINT32_SET(ctx, maxRecordSize, size);
	grn_obj_set_value(ctx, column, id, maxRecordSize, GRN_OBJ_SET);

	PGrnWALInsertColumn(walData, column, maxRecordSize);
	PGrnWALFinish(walData);
}

void
PGrnIndexStatusGetWALAppliedPosition(Relation index,
									 BlockNumber *block,
									 LocationIndex *offset)
{
	grn_id id;
	grn_obj *column;
	grn_obj *position = &(buffers->walAppliedPosition);
	uint64_t positionRaw;

	id = PGrnIndexStatusGetRecordID(index);
	column = PGrnLookup(TABLE_NAME "." WAL_APPLIED_POSITION_COLUMN_NAME,
						ERROR);
	GRN_BULK_REWIND(position);
	grn_obj_get_value(ctx, column, id, position);
	positionRaw = GRN_UINT64_VALUE(position);
	*block = (BlockNumber)(positionRaw >> 32);
	*offset = (LocationIndex)(positionRaw & ((1 << 16) - 1));
}

void
PGrnIndexStatusSetWALAppliedPosition(Relation index,
									 BlockNumber block,
									 LocationIndex offset)
{
	grn_id id;
	grn_obj *column;
	grn_obj *position = &(buffers->walAppliedPosition);
	uint64_t positionRaw;

	id = PGrnIndexStatusGetRecordID(index);
	column = PGrnLookup(TABLE_NAME "." WAL_APPLIED_POSITION_COLUMN_NAME,
						ERROR);
	positionRaw = (((uint64_t)block) << 32) + (uint64_t)offset;
	GRN_UINT64_SET(ctx, position, positionRaw);
	grn_obj_set_value(ctx, column, id, position, GRN_OBJ_SET);
}
