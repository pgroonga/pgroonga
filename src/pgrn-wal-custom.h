#pragma once

#include <postgres.h>

#include <access/xlog_internal.h>
#include <access/xloginsert.h>
#include <c.h>
#include <utils/elog.h>

#include <groonga.h>

#include "pgrn-check.h"

/* Registered at https://wiki.postgresql.org/wiki/CustomWALResourceManagers */
#define PGRN_WAL_RESOURCE_MANAGER_ID 138

#define PGRN_WAL_RECORD_CREATE_TABLE 0x10
#define PGRN_WAL_RECORD_CREATE_COLUMN 0x20
#define PGRN_WAL_RECORD_SET_SOURCES 0x30
#define PGRN_WAL_RECORD_RENAME_TABLE 0x40
#define PGRN_WAL_RECORD_INSERT 0x50
#define PGRN_WAL_RECORD_DELETE 0x60
#define PGRN_WAL_RECORD_REMOVE_OBJECT 0x70
#define PGRN_WAL_RECORD_REGISTER_PLUGIN 0x80
#define PGRN_WAL_RECORD_BULK_INSERT 0x90

#define PGRN_WAL_RECORD_DOMAIN_ID_MASK GRN_ID_MAX
#define PGRN_WAL_RECORD_DOMAIN_FLAGS_MASK 0xc0000000
#define PGRN_WAL_RECORD_DOMAIN_FLAG_VECTOR 0x40000000

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
	/* PGrnWALRecordCommon */
	Oid dbID;
	int dbEncoding;
	Oid dbTableSpaceID;

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
	record->dbID = dbID;
	record->dbEncoding = dbEncoding;
	record->dbTableSpaceID = dbTableSpaceID;
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
				   "[wal][record][read][create-table] garbage at the end: %u",
				   PGRN_TAG,
				   raw->size));
	}
}

typedef struct PGrnWALRecordCreateColumn
{
	/* PGrnWALRecordCommon */
	Oid dbID;
	int dbEncoding;
	Oid dbTableSpaceID;

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
	record->dbID = dbID;
	record->dbEncoding = dbEncoding;
	record->dbTableSpaceID = dbTableSpaceID;
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
				   "[wal][record][read][create-column] garbage at the end: %u",
				   PGRN_TAG,
				   raw->size));
	}
}

typedef struct PGrnWALRecordSetSources
{
	/* PGrnWALRecordCommon */
	Oid dbID;
	int dbEncoding;
	Oid dbTableSpaceID;

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
	record->dbID = dbID;
	record->dbEncoding = dbEncoding;
	record->dbTableSpaceID = dbTableSpaceID;
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

		PGrnWALRecordRawReadData(raw, &nameSize, sizeof(int32));
		if (nameSize == -1)
			continue;
		name = PGrnWALRecordRawRefer(raw, nameSize);
		grn_vector_add_element(
			ctx, record->sourceNames, name, nameSize, 0, GRN_DB_TEXT);
	}
	if (raw->size != 0)
	{
		ereport(
			ERROR,
			errcode(ERRCODE_DATA_EXCEPTION),
			errmsg("%s: "
				   "[wal][record][read][set-sources] garbage at the end: %u",
				   PGRN_TAG,
				   raw->size));
	}
}

typedef struct PGrnWALRecordRenameTable
{
	/* PGrnWALRecordCommon */
	Oid dbID;
	int dbEncoding;
	Oid dbTableSpaceID;

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
	record->dbID = dbID;
	record->dbEncoding = dbEncoding;
	record->dbTableSpaceID = dbTableSpaceID;
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
				   "[wal][record][read][rename-table] garbage at the end: %u",
				   PGRN_TAG,
				   raw->size));
	}
}

typedef struct PGrnWALTuple
{
	uint32 nColumns;
	grn_obj *columnNames;
	grn_obj *columnValues;
	grn_hash *columnVectorValues;
} PGrnWALTuple;

typedef struct PGrnWALRecordInsert
{
	/* PGrnWALRecordCommon */
	Oid dbID;
	int dbEncoding;
	Oid dbTableSpaceID;

	const char *tableName;
	uint32 tableNameSize;
	PGrnWALTuple tuple;
} PGrnWALRecordInsert;

static inline void
PGrnWALRecordInsertWriteStart(grn_obj *buffer,
							  PGrnWALRecordCommon *record,
							  grn_obj *table,
							  uint32 nColumns)
{
	GRN_TEXT_PUT(ctx, buffer, &(record->dbID), sizeof(Oid));
	GRN_TEXT_PUT(ctx, buffer, &(record->dbEncoding), sizeof(int));
	GRN_TEXT_PUT(ctx, buffer, &(record->dbTableSpaceID), sizeof(Oid));
	{
		char name[GRN_TABLE_MAX_KEY_SIZE];
		uint32 nameSize =
			grn_obj_name(ctx, table, name, GRN_TABLE_MAX_KEY_SIZE);
		GRN_TEXT_PUT(ctx, buffer, &nameSize, sizeof(uint32));
		GRN_TEXT_PUT(ctx, buffer, name, nameSize);
	}
	GRN_TEXT_PUT(ctx, buffer, &nColumns, sizeof(uint32));
}

static inline void
PGrnWALRecordInsertWriteColumnStart(grn_obj *buffer,
									const char *name,
									uint32 nameSize)
{
	GRN_TEXT_PUT(ctx, buffer, &nameSize, sizeof(uint32));
	GRN_TEXT_PUT(ctx, buffer, name, nameSize);
}

static inline void
PGrnWALRecordInsertWriteColumnValueKey(grn_obj *buffer,
									   const char *key,
									   uint32 keySize)
{
	GRN_TEXT_PUT(ctx, buffer, &keySize, sizeof(uint32));
	GRN_TEXT_PUT(ctx, buffer, key, keySize);
}

static inline void
PGrnWALRecordInsertWriteColumnValueRaw(grn_obj *buffer,
									   const char *name,
									   uint32 nameSize,
									   grn_id domain,
									   const char *value,
									   uint32 valueSize)
{
	switch (domain)
	{
	case GRN_DB_BOOL:
		GRN_TEXT_PUT(ctx, buffer, value, sizeof(bool));
		break;
	case GRN_DB_INT8:
		GRN_TEXT_PUT(ctx, buffer, value, sizeof(int8_t));
		break;
	case GRN_DB_UINT8:
		GRN_TEXT_PUT(ctx, buffer, value, sizeof(uint8_t));
		break;
	case GRN_DB_INT16:
		GRN_TEXT_PUT(ctx, buffer, value, sizeof(int16_t));
		break;
	case GRN_DB_UINT16:
		GRN_TEXT_PUT(ctx, buffer, value, sizeof(uint16_t));
		break;
	case GRN_DB_INT32:
		GRN_TEXT_PUT(ctx, buffer, value, sizeof(int32_t));
		break;
	case GRN_DB_UINT32:
		GRN_TEXT_PUT(ctx, buffer, value, sizeof(uint32_t));
		break;
	case GRN_DB_INT64:
		GRN_TEXT_PUT(ctx, buffer, value, sizeof(int64_t));
		break;
	case GRN_DB_UINT64:
		GRN_TEXT_PUT(ctx, buffer, value, sizeof(uint64_t));
		break;
	case GRN_DB_FLOAT:
		GRN_TEXT_PUT(ctx, buffer, value, sizeof(double));
		break;
	case GRN_DB_TIME:
		GRN_TEXT_PUT(ctx, buffer, value, sizeof(int64_t));
		break;
	case GRN_DB_SHORT_TEXT:
	case GRN_DB_TEXT:
	case GRN_DB_LONG_TEXT:
		GRN_TEXT_PUT(ctx, buffer, &valueSize, sizeof(uint32));
		GRN_TEXT_PUT(ctx, buffer, value, valueSize);
		break;
	default:
	{
		const char *tag = "[wal][insert][column][value]";
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

static inline void
PGrnWALRecordInsertWriteColumnValueBulk(grn_obj *buffer,
										const char *name,
										uint32 nameSize,
										grn_obj *value)
{
	GRN_TEXT_PUT(ctx, buffer, &(value->header.domain), sizeof(grn_id));
	PGrnWALRecordInsertWriteColumnValueRaw(buffer,
										   name,
										   nameSize,
										   value->header.domain,
										   GRN_BULK_HEAD(value),
										   GRN_BULK_VSIZE(value));
}

static inline void
PGrnWALRecordInsertWriteColumnValueVector(grn_obj *buffer,
										  const char *name,
										  uint32 nameSize,
										  grn_obj *vector)
{
	grn_id domain_and_flags =
		(vector->header.domain) | PGRN_WAL_RECORD_DOMAIN_FLAG_VECTOR;
	uint32_t i, n;

	GRN_TEXT_PUT(ctx, buffer, &domain_and_flags, sizeof(grn_id));

	n = grn_vector_size(ctx, vector);
	GRN_TEXT_PUT(ctx, buffer, &n, sizeof(uint32_t));
	for (i = 0; i < n; i++)
	{
		const char *element;
		unsigned int elementSize;
		grn_id domain;

		elementSize =
			grn_vector_get_element(ctx, vector, i, &element, NULL, &domain);
		PGrnWALRecordInsertWriteColumnValueRaw(
			buffer, name, nameSize, domain, element, elementSize);
	}
}

static inline void
PGrnWALRecordInsertWriteColumnValueUVector(grn_obj *buffer,
										   const char *name,
										   uint32 nameSize,
										   grn_obj *uvector)
{
	grn_id domain_and_flags =
		(uvector->header.domain) | PGRN_WAL_RECORD_DOMAIN_FLAG_VECTOR;
	uint32_t valueSize;

	GRN_TEXT_PUT(ctx, buffer, &domain_and_flags, sizeof(grn_id));

	valueSize = GRN_BULK_VSIZE(uvector);
	GRN_TEXT_PUT(ctx, buffer, &valueSize, sizeof(uint32_t));
	GRN_TEXT_PUT(ctx, buffer, GRN_BULK_HEAD(uvector), valueSize);
}

static inline void
PGrnWALRecordInsertWriteFinish(grn_obj *buffer)
{
	XLogBeginInsert();
	XLogRegisterData((char *) GRN_TEXT_VALUE(buffer), GRN_TEXT_LEN(buffer));
	XLogInsert(PGRN_WAL_RESOURCE_MANAGER_ID,
			   PGRN_WAL_RECORD_INSERT | XLR_SPECIAL_REL_UPDATE);
}

typedef union PGrnWALRecordInsertColumnValueBuffer {
	bool boolValue;
	int8_t int8Value;
	uint8_t uint8Value;
	int16_t int16Value;
	uint16_t uint16Value;
	int32_t int32Value;
	uint32_t uint32Value;
	int64_t int64Value;
	uint64_t uint64Value;
	double doubleValue;
} PGrnWALRecordInsertColumnValueBuffer;

static inline bool
PGrnWALRecordInsertReadColumnValueRaw(
	PGrnWALRecordRaw *raw,
	grn_id domain,
	PGrnWALRecordInsertColumnValueBuffer *valueBuffer,
	const void **value,
	uint32 *valueSize)
{
	switch (domain)
	{
	case GRN_DB_BOOL:
		*valueSize = sizeof(bool);
		PGrnWALRecordRawReadData(raw, &(valueBuffer->boolValue), *valueSize);
		*value = &(valueBuffer->boolValue);
		return true;
	case GRN_DB_INT8:
		*valueSize = sizeof(int8_t);
		PGrnWALRecordRawReadData(raw, &(valueBuffer->int8Value), *valueSize);
		*value = &(valueBuffer->int8Value);
		return true;
	case GRN_DB_UINT8:
		*valueSize = sizeof(uint8_t);
		PGrnWALRecordRawReadData(raw, &(valueBuffer->uint8Value), *valueSize);
		*value = &(valueBuffer->uint8Value);
		return true;
	case GRN_DB_INT16:
		*valueSize = sizeof(int16_t);
		PGrnWALRecordRawReadData(raw, &(valueBuffer->int16Value), *valueSize);
		*value = &(valueBuffer->int16Value);
		return true;
	case GRN_DB_UINT16:
		*valueSize = sizeof(uint16_t);
		PGrnWALRecordRawReadData(raw, &(valueBuffer->uint16Value), *valueSize);
		*value = &(valueBuffer->uint16Value);
		return true;
	case GRN_DB_INT32:
		*valueSize = sizeof(int32_t);
		PGrnWALRecordRawReadData(raw, &(valueBuffer->int32Value), *valueSize);
		*value = &(valueBuffer->int32Value);
		return true;
	case GRN_DB_UINT32:
		*valueSize = sizeof(uint32_t);
		PGrnWALRecordRawReadData(raw, &(valueBuffer->uint32Value), *valueSize);
		*value = &(valueBuffer->uint32Value);
		return true;
	case GRN_DB_INT64:
		*valueSize = sizeof(int64_t);
		PGrnWALRecordRawReadData(raw, &(valueBuffer->int64Value), *valueSize);
		*value = &(valueBuffer->int64Value);
		return true;
	case GRN_DB_UINT64:
		*valueSize = sizeof(uint64_t);
		PGrnWALRecordRawReadData(raw, &(valueBuffer->uint64Value), *valueSize);
		*value = &(valueBuffer->uint64Value);
		return true;
	case GRN_DB_FLOAT:
		*valueSize = sizeof(double);
		PGrnWALRecordRawReadData(raw, &(valueBuffer->doubleValue), *valueSize);
		*value = &(valueBuffer->doubleValue);
		return true;
	case GRN_DB_TIME:
		*valueSize = sizeof(int64_t);
		PGrnWALRecordRawReadData(raw, &(valueBuffer->int64Value), *valueSize);
		*value = &(valueBuffer->int64Value);
		return true;
	case GRN_DB_SHORT_TEXT:
	case GRN_DB_TEXT:
	case GRN_DB_LONG_TEXT:
		PGrnWALRecordRawReadData(raw, valueSize, sizeof(uint32));
		*value = PGrnWALRecordRawRefer(raw, *valueSize);
		return true;
	default:
		return false;
	}
}

static inline bool
PGrnWALRecordInsertReadColumnValueVector(PGrnWALRecordRaw *raw,
										 grn_id domain,
										 grn_obj *vectorValue)
{
	uint32_t i, n;
	GRN_OBJ_INIT(vectorValue, GRN_VECTOR, 0, domain);
	PGrnWALRecordRawReadData(raw, &n, sizeof(uint32_t));
	for (i = 0; i < n; i++)
	{
		bool succeeded;
		PGrnWALRecordInsertColumnValueBuffer valueBuffer;
		const void *value;
		uint32 valueSize;
		succeeded = PGrnWALRecordInsertReadColumnValueRaw(
			raw, domain, &valueBuffer, &value, &valueSize);
		if (!succeeded)
			return false;
		grn_vector_add_element(ctx, vectorValue, value, valueSize, 0, domain);
	}

	return true;
}

static inline bool
PGrnWALRecordInsertReadColumnValueUVector(PGrnWALRecordRaw *raw,
										  grn_id domain,
										  grn_obj *vectorValue)
{
	const void *value;
	uint32_t valueSize;
	GRN_OBJ_INIT(vectorValue, GRN_UVECTOR, GRN_OBJ_DO_SHALLOW_COPY, domain);
	PGrnWALRecordRawReadData(raw, &valueSize, sizeof(uint32_t));
	value = PGrnWALRecordRawRefer(raw, valueSize);
	GRN_TEXT_SET(ctx, vectorValue, value, valueSize);
	return true;
}

static inline void
PGrnWALRecordInsertReadColumn(PGrnWALTuple *tuple,
							  PGrnWALRecordRaw *raw,
							  uint32 i)
{
	const char *tag = "[wal][record][read][column][value]";
	const char *name;
	uint32 nameSize;
	grn_id domain;
	PGrnWALRecordInsertColumnValueBuffer valueBuffer;
	const void *value;
	uint32 valueSize;

	PGrnWALRecordRawReadData(raw, &nameSize, sizeof(uint32));
	name = PGrnWALRecordRawRefer(raw, nameSize);
	grn_vector_add_element(
		ctx, tuple->columnNames, name, nameSize, 0, GRN_DB_SHORT_TEXT);

	if (nameSize == GRN_COLUMN_NAME_KEY_LEN &&
		memcmp(name, GRN_COLUMN_NAME_KEY, GRN_COLUMN_NAME_KEY_LEN) == 0)
	{
		domain = GRN_DB_SHORT_TEXT; /* TODO: GRN_DB_SHORT_BINARY */
		PGrnWALRecordRawReadData(raw, &valueSize, sizeof(uint32));
		value = PGrnWALRecordRawRefer(raw, valueSize);
	}
	else
	{
		grn_id domain_and_flags;
		grn_id flags;
		bool isVector = false;
		bool succeeded;
		PGrnWALRecordRawReadData(raw, &domain_and_flags, sizeof(grn_id));
		domain = domain_and_flags & PGRN_WAL_RECORD_DOMAIN_ID_MASK;
		flags = domain_and_flags & PGRN_WAL_RECORD_DOMAIN_FLAGS_MASK;
		if (flags & PGRN_WAL_RECORD_DOMAIN_FLAG_VECTOR)
		{
			isVector = true;
		}
		if (isVector)
		{
			void *vectorValue;
			grn_hash_add(ctx,
						 tuple->columnVectorValues,
						 &i,
						 sizeof(uint32),
						 &vectorValue,
						 NULL);
			PGrnCheck("%s failed to allocate a vector column: <%.*s>",
					  tag,
					  (int) nameSize,
					  name);
			if (grn_type_id_is_text_family(ctx, domain))
			{
				succeeded = PGrnWALRecordInsertReadColumnValueVector(
					raw, domain, vectorValue);
			}
			else
			{
				succeeded = PGrnWALRecordInsertReadColumnValueUVector(
					raw, domain, vectorValue);
			}
			domain = GRN_DB_VOID;
			value = NULL;
			valueSize = 0;
		}
		else
		{
			succeeded = PGrnWALRecordInsertReadColumnValueRaw(
				raw, domain, &valueBuffer, &value, &valueSize);
		}
		if (!succeeded)
		{
			char domainName[GRN_TABLE_MAX_KEY_SIZE];
			int domainNameSize;

			domainNameSize = grn_table_get_key(ctx,
											   grn_ctx_db(ctx),
											   domain,
											   domainName,
											   GRN_TABLE_MAX_KEY_SIZE);
			PGrnCheckRC(GRN_FUNCTION_NOT_IMPLEMENTED,
						"%s unsupported type: <%.*s>: <%.*s>",
						tag,
						(int) nameSize,
						name,
						domainNameSize,
						domainName);
		}
	}
	grn_vector_add_element(
		ctx, tuple->columnValues, value, valueSize, 0, domain);
}

static inline void
PGrnWALRecordInsertRead(PGrnWALRecordInsert *record, PGrnWALRecordRaw *raw)
{
	uint32 i;
	PGrnWALRecordRawReadData(raw, &(record->dbID), sizeof(Oid));
	PGrnWALRecordRawReadData(raw, &(record->dbEncoding), sizeof(int));
	PGrnWALRecordRawReadData(raw, &(record->dbTableSpaceID), sizeof(Oid));
	PGrnWALRecordRawReadData(raw, &(record->tableNameSize), sizeof(uint32));
	record->tableName = PGrnWALRecordRawRefer(raw, record->tableNameSize);
	PGrnWALRecordRawReadData(raw, &(record->tuple.nColumns), sizeof(uint32));
	for (i = 0; i < record->tuple.nColumns; i++)
	{
		PGrnWALRecordInsertReadColumn(&(record->tuple), raw, i);
	}
	if (raw->size != 0)
	{
		ereport(ERROR,
				errcode(ERRCODE_DATA_EXCEPTION),
				errmsg("%s: "
					   "[wal][record][read][insert] garbage at the end: %u",
					   PGRN_TAG,
					   raw->size));
	}
}

typedef struct PGrnWALRecordDelete
{
	/* PGrnWALRecordCommon */
	Oid dbID;
	int dbEncoding;
	Oid dbTableSpaceID;

	const char *tableName;
	uint32 tableNameSize;
	const char *key;
	uint32 keySize;
} PGrnWALRecordDelete;

static inline void
PGrnWALRecordDeleteFill(PGrnWALRecordDelete *record,
						Oid dbID,
						int dbEncoding,
						Oid dbTableSpaceID,
						const char *tableName,
						uint32 tableNameSize,
						const char *key,
						uint32 keySize)
{
	record->dbID = dbID;
	record->dbEncoding = dbEncoding;
	record->dbTableSpaceID = dbTableSpaceID;
	record->tableName = tableName;
	record->tableNameSize = tableNameSize;
	record->key = key;
	record->keySize = keySize;
}

static inline void
PGrnWALRecordDeleteWrite(PGrnWALRecordDelete *record)
{
	XLogBeginInsert();
	XLogRegisterData((char *) record, offsetof(PGrnWALRecordDelete, tableName));
	XLogRegisterData((char *) &(record->tableNameSize), sizeof(uint32));
	XLogRegisterData((char *) record->tableName, record->tableNameSize);
	XLogRegisterData((char *) &(record->keySize), sizeof(uint32));
	XLogRegisterData((char *) record->key, record->keySize);
	XLogInsert(PGRN_WAL_RESOURCE_MANAGER_ID,
			   PGRN_WAL_RECORD_DELETE | XLR_SPECIAL_REL_UPDATE);
}

static inline void
PGrnWALRecordDeleteRead(PGrnWALRecordDelete *record, PGrnWALRecordRaw *raw)
{
	PGrnWALRecordRawReadData(
		raw, record, offsetof(PGrnWALRecordDelete, tableName));
	PGrnWALRecordRawReadData(raw, &(record->tableNameSize), sizeof(uint32));
	record->tableName = PGrnWALRecordRawRefer(raw, record->tableNameSize);
	PGrnWALRecordRawReadData(raw, &(record->keySize), sizeof(uint32));
	record->key = PGrnWALRecordRawRefer(raw, record->keySize);
	if (raw->size != 0)
	{
		ereport(ERROR,
				errcode(ERRCODE_DATA_EXCEPTION),
				errmsg("%s: "
					   "[wal][record][read][delete] garbage at the end: %u",
					   PGRN_TAG,
					   raw->size));
	}
}

typedef struct PGrnWALRecordRemoveObject
{
	/* PGrnWALRecordCommon */
	Oid dbID;
	int dbEncoding;
	Oid dbTableSpaceID;

	const char *name;
	uint32 nameSize;
} PGrnWALRecordRemoveObject;

static inline void
PGrnWALRecordRemoveObjectFill(PGrnWALRecordRemoveObject *record,
							  Oid dbID,
							  int dbEncoding,
							  Oid dbTableSpaceID,
							  const char *name,
							  uint32 nameSize)
{
	record->dbID = dbID;
	record->dbEncoding = dbEncoding;
	record->dbTableSpaceID = dbTableSpaceID;
	record->name = name;
	record->nameSize = nameSize;
}

static inline void
PGrnWALRecordRemoveObjectWrite(PGrnWALRecordRemoveObject *record)
{
	XLogBeginInsert();
	XLogRegisterData((char *) record,
					 offsetof(PGrnWALRecordRemoveObject, name));
	XLogRegisterData((char *) &(record->nameSize), sizeof(uint32));
	XLogRegisterData((char *) record->name, record->nameSize);
	XLogInsert(PGRN_WAL_RESOURCE_MANAGER_ID,
			   PGRN_WAL_RECORD_REMOVE_OBJECT | XLR_SPECIAL_REL_UPDATE);
}

static inline void
PGrnWALRecordRemoveObjectRead(PGrnWALRecordRemoveObject *record,
							  PGrnWALRecordRaw *raw)
{
	PGrnWALRecordRawReadData(
		raw, record, offsetof(PGrnWALRecordRemoveObject, name));
	PGrnWALRecordRawReadData(raw, &(record->nameSize), sizeof(uint32));
	record->name = PGrnWALRecordRawRefer(raw, record->nameSize);
	if (raw->size != 0)
	{
		ereport(
			ERROR,
			errcode(ERRCODE_DATA_EXCEPTION),
			errmsg("%s: "
				   "[wal][record][read][remove-object] garbage at the end: %u",
				   PGRN_TAG,
				   raw->size));
	}
}

typedef struct PGrnWALRecordRegisterPlugin
{
	/* PGrnWALRecordCommon */
	Oid dbID;
	int dbEncoding;
	Oid dbTableSpaceID;

	const char *name;
	uint32 nameSize;
} PGrnWALRecordRegisterPlugin;

static inline void
PGrnWALRecordRegisterPluginFill(PGrnWALRecordRegisterPlugin *record,
								Oid dbID,
								int dbEncoding,
								Oid dbTableSpaceID,
								const char *name,
								uint32 nameSize)
{
	record->dbID = dbID;
	record->dbEncoding = dbEncoding;
	record->dbTableSpaceID = dbTableSpaceID;
	record->name = name;
	record->nameSize = nameSize;
}

static inline void
PGrnWALRecordRegisterPluginWrite(PGrnWALRecordRegisterPlugin *record)
{
	XLogBeginInsert();
	XLogRegisterData((char *) record,
					 offsetof(PGrnWALRecordRegisterPlugin, name));
	XLogRegisterData((char *) &(record->nameSize), sizeof(uint32));
	XLogRegisterData((char *) record->name, record->nameSize);
	XLogInsert(PGRN_WAL_RESOURCE_MANAGER_ID,
			   PGRN_WAL_RECORD_REGISTER_PLUGIN | XLR_SPECIAL_REL_UPDATE);
}

static inline void
PGrnWALRecordRegisterPluginRead(PGrnWALRecordRegisterPlugin *record,
								PGrnWALRecordRaw *raw)
{
	PGrnWALRecordRawReadData(
		raw, record, offsetof(PGrnWALRecordRegisterPlugin, name));
	PGrnWALRecordRawReadData(raw, &(record->nameSize), sizeof(uint32));
	record->name = PGrnWALRecordRawRefer(raw, record->nameSize);
	if (raw->size != 0)
	{
		ereport(
			ERROR,
			errcode(ERRCODE_DATA_EXCEPTION),
			errmsg(
				"%s: "
				"[wal][record][read][register-plugin] garbage at the end: %u",
				PGRN_TAG,
				raw->size));
	}
}

typedef struct PGrnWALRecordBulkInsert
{
	/* PGrnWALRecordCommon */
	Oid dbID;
	int dbEncoding;
	Oid dbTableSpaceID;

	/* Shared by all records */
	const char *tableName;
	uint32 tableNameSize;

	/* For each record */
	PGrnWALTuple tuple;
} PGrnWALRecordBulkInsert;

static inline void
PGrnWALRecordBulkInsertWriteStart(grn_obj *buffer,
								  PGrnWALRecordCommon *record,
								  grn_obj *table)
{
	GRN_TEXT_PUT(ctx, buffer, &(record->dbID), sizeof(Oid));
	GRN_TEXT_PUT(ctx, buffer, &(record->dbEncoding), sizeof(int));
	GRN_TEXT_PUT(ctx, buffer, &(record->dbTableSpaceID), sizeof(Oid));
	{
		char name[GRN_TABLE_MAX_KEY_SIZE];
		uint32 nameSize =
			grn_obj_name(ctx, table, name, GRN_TABLE_MAX_KEY_SIZE);
		GRN_TEXT_PUT(ctx, buffer, &nameSize, sizeof(uint32));
		GRN_TEXT_PUT(ctx, buffer, name, nameSize);
	}
}

static inline void
PGrnWALRecordBulkInsertWriteRecordStart(grn_obj *buffer,
										PGrnWALRecordCommon *record,
										uint32 nColumns)
{
	GRN_TEXT_PUT(ctx, buffer, &nColumns, sizeof(uint32));
}

static inline void
PGrnWALRecordBulkInsertWriteColumnStart(grn_obj *buffer,
										const char *name,
										uint32 nameSize)
{
	PGrnWALRecordInsertWriteColumnStart(buffer, name, nameSize);
}

static inline void
PGrnWALRecordBulkInsertWriteColumnValueKey(grn_obj *buffer,
										   const char *key,
										   uint32 keySize)
{
	PGrnWALRecordInsertWriteColumnValueKey(buffer, key, keySize);
}

static inline void
PGrnWALRecordBulkInsertWriteColumnValueBulk(grn_obj *buffer,
											const char *name,
											uint32 nameSize,
											grn_obj *value)
{
	PGrnWALRecordInsertWriteColumnValueBulk(buffer, name, nameSize, value);
}

static inline void
PGrnWALRecordBulkInsertWriteColumnValueVector(grn_obj *buffer,
											  const char *name,
											  uint32 nameSize,
											  grn_obj *vector)
{
	PGrnWALRecordInsertWriteColumnValueVector(buffer, name, nameSize, vector);
}

static inline void
PGrnWALRecordBulkInsertWriteColumnValueUVector(grn_obj *buffer,
											   const char *name,
											   uint32 nameSize,
											   grn_obj *uvector)
{
	PGrnWALRecordInsertWriteColumnValueUVector(buffer, name, nameSize, uvector);
}

static inline void
PGrnWALRecordBulkInsertWriteRecordFinish(grn_obj *buffer)
{
}

static inline void
PGrnWALRecordBulkInsertWriteFinish(grn_obj *buffer)
{
	XLogBeginInsert();
	XLogRegisterData((char *) GRN_TEXT_VALUE(buffer), GRN_TEXT_LEN(buffer));
	XLogInsert(PGRN_WAL_RESOURCE_MANAGER_ID,
			   PGRN_WAL_RECORD_BULK_INSERT | XLR_SPECIAL_REL_UPDATE);
}

static inline void
PGrnWALRecordBulkInsertRead(PGrnWALRecordBulkInsert *record,
							PGrnWALRecordRaw *raw)
{
	PGrnWALRecordRawReadData(raw, &(record->dbID), sizeof(Oid));
	PGrnWALRecordRawReadData(raw, &(record->dbEncoding), sizeof(int));
	PGrnWALRecordRawReadData(raw, &(record->dbTableSpaceID), sizeof(Oid));
	PGrnWALRecordRawReadData(raw, &(record->tableNameSize), sizeof(uint32));
	record->tableName = PGrnWALRecordRawRefer(raw, record->tableNameSize);
}

static inline bool
PGrnWALRecordBulkInsertReadNext(PGrnWALRecordBulkInsert *record,
								PGrnWALRecordRaw *raw)
{
	uint32 i;

	if (raw->size == 0)
	{
		return false;
	}

	PGrnWALRecordRawReadData(raw, &(record->tuple.nColumns), sizeof(uint32));
	for (i = 0; i < record->tuple.nColumns; i++)
	{
		PGrnWALRecordInsertReadColumn(&(record->tuple), raw, i);
	}
	return true;
}
