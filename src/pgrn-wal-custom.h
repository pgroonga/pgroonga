#pragma once

#include <postgres.h>

#include <access/xlog_internal.h>
#include <access/xloginsert.h>
#include <c.h>
#include <utils/elog.h>

#include <groonga.h>

#define PGRN_WAL_RESOURCE_MANAGER_ID RM_EXPERIMENTAL_ID

#define PGRN_WAL_RECORD_CREATE_TABLE 0x10
#define PGRN_WAL_RECORD_CREATE_COLUMN 0x20
#define PGRN_WAL_RECORD_SET_SOURCES 0x30
#define PGRN_WAL_RECORD_RENAME_TABLE 0x40
#define PGRN_WAL_RECORD_INSERT 0x50
#define PGRN_WAL_RECORD_DELETE 0x60

typedef struct PGrnWALRecordRaw
{
	const char *data;
	uint32 size;
} PGrnWALRecordRaw;

static inline const void *
PGrnWALRecordRawRefer(PGrnWALRecordRaw *raw, uint32 size)
{
	const char *data;
	if (raw->size < size)
	{
		ereport(ERROR,
				errcode(ERRCODE_DATA_EXCEPTION),
				errmsg("%s: [wal][record][refar][data] small data: "
					   "expected:%u actual:%u",
					   PGRN_TAG,
					   size,
					   raw->size));
	}
	data = raw->data;
	raw->data += size;
	raw->size -= size;
	return data;
}

static inline void
PGrnWALRecordRawReadData(PGrnWALRecordRaw *raw, void *output, uint32 size)
{
	if (raw->size < size)
	{
		ereport(ERROR,
				errcode(ERRCODE_DATA_EXCEPTION),
				errmsg("%s: [wal][record][read][data] small data: "
					   "expected:%u actual:%u",
					   PGRN_TAG,
					   size,
					   raw->size));
	}
	memcpy(output, raw->data, size);
	raw->data += size;
	raw->size -= size;
}

static inline void
PGrnWALRecordRawReadGrnObj(PGrnWALRecordRaw *raw, grn_obj *object)
{
	int32 size;
	if (raw->size < sizeof(int32))
	{
		ereport(ERROR,
				errcode(ERRCODE_DATA_EXCEPTION),
				errmsg("pgroonga: wal-resource-manager: "
					   "[wal][record][read][object][size] small data: "
					   "expected:%zu actual:%u",
					   sizeof(int32),
					   raw->size));
	}
	memcpy(&size, raw->data, sizeof(int32));
	raw->data += sizeof(int32);
	raw->size -= sizeof(int32);
	if (size < 0)
	{
		grn_obj_reinit(ctx, object, GRN_DB_VOID, 0);
	}
	else
	{
		if (raw->size < (uint32) size)
		{
			ereport(ERROR,
					errcode(ERRCODE_DATA_EXCEPTION),
					errmsg("pgroonga: wal-resource-manager: "
						   "[wal][record][read][object][value] small data: "
						   "expected:%d actual:%u",
						   size,
						   raw->size));
		}
		GRN_TEXT_SET(ctx, object, raw->data, size);
		raw->data += size;
		raw->size -= size;
	}
}

static inline void
PGrnWALRecordWriteGrnObj(grn_obj *object, char *buffer, int32 *size)
{
	if (object)
	{
		if (grn_obj_is_text_family_bulk(ctx, object))
		{
			*size = GRN_TEXT_LEN(object);
			XLogRegisterData((char *) size, sizeof(int32));
			XLogRegisterData(GRN_TEXT_VALUE(object), GRN_TEXT_LEN(object));
		}
		else
		{
			*size = grn_obj_name(ctx, object, buffer, GRN_TABLE_MAX_KEY_SIZE);
			XLogRegisterData((char *) size, sizeof(int32));
			XLogRegisterData(buffer, *size);
		}
	}
	else
	{
		*size = -1;
		XLogRegisterData((char *) size, sizeof(int32));
	}
}

typedef struct PGrnWALRecordCommon
{
	Oid dbID;
	int dbEncoding;
	Oid dbTableSpaceID;
} PGrnWALRecordCommon;

typedef struct PGrnWALRecordCreateTable
{
	PGrnWALRecordCommon common;
	Oid indexTableSpaceID;
	int32 nameSize;
	const char *name;
	grn_table_flags flags;
	grn_obj *type;
	grn_obj *tokenizer;
	grn_obj *normalizers;
	grn_obj *tokenFilters;
} PGrnWALRecordCreateTable;

static inline void
PGrnWALRecordCreateTableFill(PGrnWALRecordCreateTable *record,
							 Oid dbID,
							 int dbEncoding,
							 Oid dbTableSpaceID,
							 Oid indexTableSpaceID,
							 const char *name,
							 size_t nameSize,
							 grn_table_flags flags,
							 grn_obj *type,
							 grn_obj *tokenizer,
							 grn_obj *normalizers,
							 grn_obj *tokenFilters)
{
	record->common.dbID = dbID;
	record->common.dbEncoding = dbEncoding;
	record->common.dbTableSpaceID = dbTableSpaceID;
	record->indexTableSpaceID = indexTableSpaceID;
	record->nameSize = (int32) nameSize;
	record->name = name;
	record->flags = flags;
	record->type = type;
	record->tokenizer = tokenizer;
	record->normalizers = normalizers;
	record->tokenFilters = tokenFilters;
}

static inline void
PGrnWALRecordCreateTableWrite(PGrnWALRecordCreateTable *record)
{
	char typeNameBuffer[GRN_TABLE_MAX_KEY_SIZE];
	int32 typeNameSize;
	char tokenizerNameBuffer[GRN_TABLE_MAX_KEY_SIZE];
	int32 tokenizerNameSize;
	char normalizersNameBuffer[GRN_TABLE_MAX_KEY_SIZE];
	int32 normalizersNameSize;
	char tokenFiltersNameBuffer[GRN_TABLE_MAX_KEY_SIZE];
	int32 tokenFiltersNameSize;
	XLogBeginInsert();
	XLogRegisterData((char *) record, offsetof(PGrnWALRecordCreateTable, name));
	XLogRegisterData((char *) (record->name), record->nameSize);
	XLogRegisterData((char *) &(record->flags), sizeof(grn_table_flags));
	PGrnWALRecordWriteGrnObj(record->type, typeNameBuffer, &typeNameSize);
	PGrnWALRecordWriteGrnObj(
		record->tokenizer, tokenizerNameBuffer, &tokenizerNameSize);
	PGrnWALRecordWriteGrnObj(
		record->normalizers, normalizersNameBuffer, &normalizersNameSize);
	PGrnWALRecordWriteGrnObj(
		record->tokenFilters, tokenFiltersNameBuffer, &tokenFiltersNameSize);
	XLogInsert(PGRN_WAL_RESOURCE_MANAGER_ID,
			   PGRN_WAL_RECORD_CREATE_TABLE | XLR_SPECIAL_REL_UPDATE);
}

static inline void
PGrnWALRecordCreateTableRead(PGrnWALRecordCreateTable *record,
							 PGrnWALRecordRaw *raw)
{
	PGrnWALRecordRawReadData(
		raw, record, offsetof(PGrnWALRecordCreateTable, name));
	record->name = PGrnWALRecordRawRefer(raw, record->nameSize);
	PGrnWALRecordRawReadData(raw, &(record->flags), sizeof(grn_table_flags));
	PGrnWALRecordRawReadGrnObj(raw, record->type);
	PGrnWALRecordRawReadGrnObj(raw, record->tokenizer);
	PGrnWALRecordRawReadGrnObj(raw, record->normalizers);
	PGrnWALRecordRawReadGrnObj(raw, record->tokenFilters);
	if (raw->size != 0)
	{
		ereport(
			ERROR,
			errcode(ERRCODE_DATA_EXCEPTION),
			errmsg("%s: "
				   "[wal][record][read][create-table] garbage at the end:%u",
				   PGRN_TAG,
				   raw->size));
	}
}

typedef struct PGrnWALRecordCreateColumn
{
	PGrnWALRecordCommon common;
	Oid indexTableSpaceID;
	grn_obj *table;
	int32 nameSize;
	const char *name;
	grn_column_flags flags;
	grn_obj *type;
} PGrnWALRecordCreateColumn;

static inline void
PGrnWALRecordCreateColumnFill(PGrnWALRecordCreateColumn *record,
							  Oid dbID,
							  int dbEncoding,
							  Oid dbTableSpaceID,
							  Oid indexTableSpaceID,
							  grn_obj *table,
							  const char *name,
							  size_t nameSize,
							  grn_column_flags flags,
							  grn_obj *type)
{
	record->common.dbID = dbID;
	record->common.dbEncoding = dbEncoding;
	record->common.dbTableSpaceID = dbTableSpaceID;
	record->indexTableSpaceID = indexTableSpaceID;
	record->table = table;
	record->nameSize = (int32) nameSize;
	record->name = name;
	record->flags = flags;
	record->type = type;
}

static inline void
PGrnWALRecordCreateColumnWrite(PGrnWALRecordCreateColumn *record)
{
	char tableNameBuffer[GRN_TABLE_MAX_KEY_SIZE];
	int32 tableNameSize;
	char typeNameBuffer[GRN_TABLE_MAX_KEY_SIZE];
	int32 typeNameSize;
	XLogBeginInsert();
	XLogRegisterData((char *) record,
					 offsetof(PGrnWALRecordCreateColumn, table));
	PGrnWALRecordWriteGrnObj(record->table, tableNameBuffer, &tableNameSize);
	XLogRegisterData((char *) &(record->nameSize), sizeof(int32));
	XLogRegisterData((char *) (record->name), record->nameSize);
	XLogRegisterData((char *) &(record->flags), sizeof(grn_column_flags));
	PGrnWALRecordWriteGrnObj(record->type, typeNameBuffer, &typeNameSize);
	XLogInsert(PGRN_WAL_RESOURCE_MANAGER_ID,
			   PGRN_WAL_RECORD_CREATE_COLUMN | XLR_SPECIAL_REL_UPDATE);
}

static inline void
PGrnWALRecordCreateColumnRead(PGrnWALRecordCreateColumn *record,
							  PGrnWALRecordRaw *raw)
{
	PGrnWALRecordRawReadData(
		raw, record, offsetof(PGrnWALRecordCreateColumn, table));
	PGrnWALRecordRawReadGrnObj(raw, record->table);
	PGrnWALRecordRawReadData(raw, &(record->nameSize), sizeof(int32));
	record->name = PGrnWALRecordRawRefer(raw, record->nameSize);
	PGrnWALRecordRawReadData(raw, &(record->flags), sizeof(grn_column_flags));
	PGrnWALRecordRawReadGrnObj(raw, record->type);
	if (raw->size != 0)
	{
		ereport(
			ERROR,
			errcode(ERRCODE_DATA_EXCEPTION),
			errmsg("%s: "
				   "[wal][record][read][create-column] garbage at the end:%u",
				   PGRN_TAG,
				   raw->size));
	}
}

typedef struct PGrnWALRecordSetSources
{
	PGrnWALRecordCommon common;
	grn_obj *column;
	grn_obj *sourceIDs;
	grn_obj *sourceNames;
} PGrnWALRecordSetSources;

static inline void
PGrnWALRecordSetSourcesFill(PGrnWALRecordSetSources *record,
							Oid dbID,
							int dbEncoding,
							Oid dbTableSpaceID,
							grn_obj *column,
							grn_obj *sourceIDs)
{
	record->common.dbID = dbID;
	record->common.dbEncoding = dbEncoding;
	record->common.dbTableSpaceID = dbTableSpaceID;
	record->column = column;
	record->sourceIDs = sourceIDs;
	record->sourceNames = NULL;
}

static inline void
PGrnWALRecordSetSourcesWrite(PGrnWALRecordSetSources *record)
{
	char columnNameBuffer[GRN_TABLE_MAX_KEY_SIZE];
	int32 columnNameSize;
	grn_obj sourceNames;
	grn_obj sourceNameSizes;
	size_t i;
	size_t n = GRN_RECORD_VECTOR_SIZE(record->sourceIDs);

	XLogBeginInsert();
	XLogRegisterData((char *) record,
					 offsetof(PGrnWALRecordSetSources, column));
	PGrnWALRecordWriteGrnObj(record->column, columnNameBuffer, &columnNameSize);
	XLogRegisterData((char *) &n, sizeof(size_t));
	GRN_TEXT_INIT(&sourceNames, GRN_OBJ_VECTOR);
	GRN_INT32_INIT(&sourceNameSizes, GRN_OBJ_VECTOR);
	for (i = 0; i < n; i++)
	{
		grn_id sourceID = GRN_RECORD_VALUE_AT(record->sourceIDs, i);
		grn_obj *source = grn_ctx_at(ctx, sourceID);
		if (source)
		{
			char name[GRN_TABLE_MAX_KEY_SIZE];
			int nameSize =
				grn_obj_name(ctx, source, name, GRN_TABLE_MAX_KEY_SIZE);
			grn_vector_add_element(
				ctx, &sourceNames, name, nameSize, 0, GRN_DB_TEXT);
			GRN_INT32_PUT(ctx, &sourceNameSizes, nameSize);
		}
		else
		{
			grn_vector_add_element(ctx, &sourceNames, NULL, 0, 0, GRN_DB_TEXT);
			GRN_INT32_PUT(ctx, &sourceNameSizes, -1);
		}
	}
	for (i = 0; i < n; i++)
	{
		int32_t *rawNameSizes = (int32_t *) GRN_BULK_HEAD(&sourceNameSizes);
		int32_t nameSize = rawNameSizes[i];
		XLogRegisterData((char *) &(rawNameSizes[i]), sizeof(int32));
		if (nameSize != -1)
		{
			const char *name;
			grn_vector_get_element(ctx, &sourceNames, i, &name, NULL, NULL);
			XLogRegisterData((char *) name, nameSize);
		}
	}
	XLogInsert(PGRN_WAL_RESOURCE_MANAGER_ID,
			   PGRN_WAL_RECORD_SET_SOURCES | XLR_SPECIAL_REL_UPDATE);
}

static inline void
PGrnWALRecordSetSourcesRead(PGrnWALRecordSetSources *record,
							PGrnWALRecordRaw *raw)
{
	size_t i;
	size_t n;

	PGrnWALRecordRawReadData(
		raw, record, offsetof(PGrnWALRecordSetSources, column));
	PGrnWALRecordRawReadGrnObj(raw, record->column);
	PGrnWALRecordRawReadData(raw, &n, sizeof(size_t));
	for (i = 0; i < n; i++)
	{
		const char *name;
		int32 nameSize;
		grn_obj *source;
		grn_id sourceID;

		PGrnWALRecordRawReadData(raw, &nameSize, sizeof(int32));
		if (nameSize == -1)
			continue;
		name = PGrnWALRecordRawRefer(raw, nameSize);
		grn_vector_add_element(
			ctx, record->sourceNames, name, nameSize, 0, GRN_DB_TEXT);
	}
	if (raw->size != 0)
	{
		ereport(ERROR,
				errcode(ERRCODE_DATA_EXCEPTION),
				errmsg("%s: "
					   "[wal][record][read][set-sources] garbage at the end:%u",
					   PGRN_TAG,
					   raw->size));
	}
}

typedef struct PGrnWALRecordRenameTable
{
	PGrnWALRecordCommon common;
	const char *name;
	uint32 nameSize;
	const char *newName;
	uint32 newNameSize;
} PGrnWALRecordRenameTable;

static inline void
PGrnWALRecordRenameTableFill(PGrnWALRecordRenameTable *record,
							 Oid dbID,
							 int dbEncoding,
							 Oid dbTableSpaceID,
							 const char *name,
							 uint32 nameSize,
							 const char *newName,
							 uint32 newNameSize)
{
	record->common.dbID = dbID;
	record->common.dbEncoding = dbEncoding;
	record->common.dbTableSpaceID = dbTableSpaceID;
	record->name = name;
	record->nameSize = nameSize;
	record->newName = newName;
	record->newNameSize = newNameSize;
}

static inline void
PGrnWALRecordRenameTableWrite(PGrnWALRecordRenameTable *record)
{
	XLogBeginInsert();
	XLogRegisterData((char *) record, offsetof(PGrnWALRecordRenameTable, name));
	XLogRegisterData((char *) &(record->nameSize), sizeof(uint32));
	XLogRegisterData((char *) record->name, record->nameSize);
	XLogRegisterData((char *) &(record->newNameSize), sizeof(uint32));
	XLogRegisterData((char *) record->newName, record->newNameSize);
	XLogInsert(PGRN_WAL_RESOURCE_MANAGER_ID,
			   PGRN_WAL_RECORD_RENAME_TABLE | XLR_SPECIAL_REL_UPDATE);
}

static inline void
PGrnWALRecordRenameTableRead(PGrnWALRecordRenameTable *record,
							 PGrnWALRecordRaw *raw)
{
	PGrnWALRecordRawReadData(
		raw, record, offsetof(PGrnWALRecordRenameTable, name));
	PGrnWALRecordRawReadData(raw, &(record->nameSize), sizeof(uint32));
	record->name = PGrnWALRecordRawRefer(raw, record->nameSize);
	PGrnWALRecordRawReadData(raw, &(record->newNameSize), sizeof(uint32));
	record->newName = PGrnWALRecordRawRefer(raw, record->newNameSize);
	if (raw->size != 0)
	{
		ereport(
			ERROR,
			errcode(ERRCODE_DATA_EXCEPTION),
			errmsg("%s: "
				   "[wal][record][read][rename-table] garbage at the end:%u",
				   PGRN_TAG,
				   raw->size));
	}
}
