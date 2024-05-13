#pragma once

#include "pgrn-check.h"
#include "pgrn-column-name.h"

#include <c.h>
#include <miscadmin.h>
#include <postgres.h>
#include <utils/rel.h>

extern bool PGrnIsLZ4Available;
extern bool PGrnIsZlibAvailable;
extern bool PGrnIsZstdAvailable;
extern bool PGrnIsTemporaryIndexSearchAvailable;

void PGrnInitializeGroongaInformation(void);

static inline const char *
PGrnInspect(grn_obj *object)
{
	grn_obj *buffer = &PGrnInspectBuffer;

	GRN_BULK_REWIND(buffer);
	{
		grn_rc rc = ctx->rc;
		grn_inspect(ctx, buffer, object);
		ctx->rc = rc;
	}
	GRN_TEXT_PUTC(ctx, buffer, '\0');
	return GRN_TEXT_VALUE(buffer);
}

static inline const char *
PGrnInspectKey(grn_obj *table, const void *key, uint32_t keySize)
{
	grn_obj *buffer = &PGrnInspectBuffer;

	GRN_BULK_REWIND(buffer);
	{
		grn_rc rc = ctx->rc;
		grn_inspect_key(ctx, buffer, table, key, keySize);
		ctx->rc = rc;
	}
	GRN_TEXT_PUTC(ctx, buffer, '\0');
	return GRN_TEXT_VALUE(buffer);
}

static inline const char *
PGrnInspectName(grn_obj *object)
{
	static char name[GRN_TABLE_MAX_KEY_SIZE];
	int nameSize;

	{
		grn_rc rc = ctx->rc;
		nameSize = grn_obj_name(ctx, object, name, GRN_TABLE_MAX_KEY_SIZE);
		name[nameSize] = '\0';
		ctx->rc = rc;
	}

	return name;
}

static inline grn_encoding
PGrnPGEncodingToGrnEncoding(int pgEncoding)
{
	switch (pgEncoding)
	{
	case PG_SQL_ASCII:
	case PG_UTF8:
		return GRN_ENC_UTF8;
	case PG_EUC_JP:
	case PG_EUC_JIS_2004:
		return GRN_ENC_EUC_JP;
	case PG_LATIN1:
	case PG_WIN1252:
		return GRN_ENC_LATIN1;
	case PG_KOI8R:
		return GRN_ENC_KOI8R;
	case PG_SJIS:
	case PG_SHIFT_JIS_2004:
		return GRN_ENC_SJIS;
	default:
		ereport(WARNING,
				errmsg("%s: use default encoding instead of '%s'",
					   PGRN_TAG,
					   pg_encoding_to_char(pgEncoding)));
		return GRN_ENC_DEFAULT;
	}
}

grn_obj *PGrnLookup(const char *name, int errorLevel);

static inline grn_obj *
PGrnLookupWithSize(const char *name, size_t nameSize, int errorLevel)
{
	grn_obj *object;
	object = grn_ctx_get(ctx, name, nameSize);
	if (!object && errorLevel != PGRN_ERROR_LEVEL_IGNORE)
	{
		PGrnCheckRCLevel(GRN_INVALID_ARGUMENT,
						 errorLevel,
						 "object isn't found: <%.*s>",
						 (int) nameSize,
						 name);
	}
	return object;
}

grn_obj *PGrnLookupColumn(grn_obj *table, const char *name, int errorLevel);

static inline grn_obj *
PGrnLookupColumnWithSize(grn_obj *table,
						 const char *name,
						 size_t nameSize,
						 int errorLevel)
{
	char columnName[GRN_TABLE_MAX_KEY_SIZE];
	size_t columnNameSize;
	grn_obj *column;

	columnNameSize = PGrnColumnNameEncodeWithSize(name, nameSize, columnName);
	column = grn_obj_column(ctx, table, columnName, columnNameSize);
	if (!column)
	{
		char tableName[GRN_TABLE_MAX_KEY_SIZE];
		int tableNameSize;

		tableNameSize = grn_obj_name(ctx, table, tableName, sizeof(tableName));
		PGrnCheckRCLevel(GRN_INVALID_ARGUMENT,
						 errorLevel,
						 "column isn't found: <%.*s>:<%.*s>",
						 tableNameSize,
						 tableName,
						 (int) nameSize,
						 name);
	}

	return column;
}

grn_obj *PGrnLookupSourcesTable(Relation index, int errorLevel);
grn_obj *PGrnLookupSourcesCtidColumn(Relation index, int errorLevel);
grn_obj *
PGrnLookupLexicon(Relation index, unsigned int nthAttribute, int errorLevel);
grn_obj *PGrnLookupIndexColumn(Relation index,
							   unsigned int nthAttribute,
							   int errorLevel);

void PGrnFormatSourcesTableName(const char *indexName,
								char output[GRN_TABLE_MAX_KEY_SIZE]);

static inline bool
PGrnTableModuleValueIsPresent(grn_obj *module)
{
	if (!module)
		return false;
	if (module->header.type == GRN_DB_VOID)
		return false;
	if (!grn_obj_is_text_family_bulk(ctx, module))
		return true;
	if (GRN_TEXT_LEN(module) == 0)
		return false;
	return true;
}

static inline grn_obj *
PGrnCreateTableRawWithSize(Oid tableSpaceID,
						   const char *name,
						   size_t nameSize,
						   grn_table_flags flags,
						   grn_obj *type,
						   grn_obj *tokenizer,
						   grn_obj *normalizers,
						   grn_obj *tokenFilters)
{
	const char *path = NULL;
	char pathBuffer[MAXPGPATH];
	grn_obj *table;

	if (name)
	{
		flags |= GRN_OBJ_PERSISTENT;
		if (tableSpaceID != InvalidOid)
		{
			char *databasePath;
			char filePath[MAXPGPATH];

			databasePath = GetDatabasePath(MyDatabaseId, tableSpaceID);
			snprintf(filePath,
					 sizeof(filePath),
					 "%s.%.*s",
					 PGrnDatabaseBasename,
					 (int) nameSize,
					 name);
			join_path_components(pathBuffer, databasePath, filePath);
			pfree(databasePath);

			path = pathBuffer;
		}
	}

	table = grn_table_create(ctx, name, nameSize, path, flags, type, NULL);
	PGrnCheck("failed to create table: <%.*s>", (int) nameSize, name);
	if (PGrnTableModuleValueIsPresent(tokenizer))
	{
		grn_obj_set_info(ctx, table, GRN_INFO_DEFAULT_TOKENIZER, tokenizer);
		PGrnCheck("failed to set tokenizer: <%s>", PGrnInspect(tokenizer));
	}
	if (PGrnTableModuleValueIsPresent(normalizers))
	{
		grn_obj_set_info(ctx, table, GRN_INFO_NORMALIZERS, normalizers);
		PGrnCheck("failed to set normalizers: <%s>", PGrnInspect(normalizers));
	}
	if (PGrnTableModuleValueIsPresent(tokenFilters))
	{
		grn_obj_set_info(ctx, table, GRN_INFO_TOKEN_FILTERS, tokenFilters);
		PGrnCheck("failed to set token filters: <%s>", PGrnInspect(tokenFilters));
	}

	return table;
}

grn_obj *PGrnCreateTable(Relation index,
						 const char *name,
						 grn_table_flags flags,
						 grn_obj *type,
						 grn_obj *tokenizer,
						 grn_obj *normalizers,
						 grn_obj *tokenFilters);
grn_obj *PGrnCreateTableWithSize(Relation index,
								 const char *name,
								 size_t nameSize,
								 grn_table_flags flags,
								 grn_obj *type,
								 grn_obj *tokenizer,
								 grn_obj *normalizers,
								 grn_obj *tokenFilters);
grn_obj *PGrnCreateSimilarTemporaryLexicon(Relation index,
										   const char *attributeName,
										   size_t attributeNameSize,
										   const char *tag);

static inline grn_obj *
PGrnCreateColumnRawWithSize(Oid tableSpaceID,
							grn_obj *table,
							const char *name,
							size_t nameSize,
							grn_column_flags flags,
							grn_obj *type)
{
	const char *path = NULL;
	char pathBuffer[MAXPGPATH];
	grn_obj *column;

	if (name)
	{
		flags |= GRN_OBJ_PERSISTENT;
		if (tableSpaceID != InvalidOid)
		{
			char *databasePath;
			char tableName[GRN_TABLE_MAX_KEY_SIZE];
			int tableNameSize;
			char filePath[MAXPGPATH];

			databasePath = GetDatabasePath(MyDatabaseId, tableSpaceID);
			tableNameSize =
				grn_obj_name(ctx, table, tableName, GRN_TABLE_MAX_KEY_SIZE);
			snprintf(filePath,
					 sizeof(filePath),
					 "%s.%.*s.%.*s",
					 PGrnDatabaseBasename,
					 tableNameSize,
					 tableName,
					 (int) nameSize,
					 name);
			join_path_components(pathBuffer, databasePath, filePath);
			pfree(databasePath);

			path = pathBuffer;
		}
	}
	column = grn_column_create(ctx, table, name, nameSize, path, flags, type);
	PGrnCheck("failed to create column: <%.*s>", (int) nameSize, name);

	return column;
}

grn_obj *PGrnCreateColumn(Relation index,
						  grn_obj *table,
						  const char *name,
						  grn_column_flags flags,
						  grn_obj *type);
grn_obj *PGrnCreateColumnWithSize(Relation index,
								  grn_obj *table,
								  const char *name,
								  size_t nameSize,
								  grn_column_flags flags,
								  grn_obj *type);

void PGrnIndexColumnClearSources(Relation index, grn_obj *indexColumn);
void
PGrnIndexColumnSetSource(Relation index, grn_obj *indexColumn, grn_obj *source);

static inline bool
PGrnIndexColumnSetSourceIDsRaw(grn_obj *indexColumn, grn_obj *sourceIDs)
{
	grn_obj currentSourceIDs;
	bool same;

	GRN_RECORD_INIT(&currentSourceIDs, GRN_OBJ_VECTOR, GRN_ID_NIL);
	grn_obj_get_info(ctx, indexColumn, GRN_INFO_SOURCE, &currentSourceIDs);
	same = (GRN_BULK_VSIZE(&currentSourceIDs) == GRN_BULK_VSIZE(sourceIDs) &&
			memcmp(GRN_BULK_HEAD(&currentSourceIDs),
				   GRN_BULK_HEAD(sourceIDs),
				   GRN_BULK_VSIZE(&currentSourceIDs)) == 0);
	GRN_OBJ_FIN(ctx, &currentSourceIDs);
	if (same)
	{
		return false;
	}

	grn_obj_set_info(ctx, indexColumn, GRN_INFO_SOURCE, sourceIDs);
	PGrnCheck("failed to set sources: <%s>: <%s>",
			  PGrnInspectName(indexColumn),
			  PGrnInspect(sourceIDs));

	return true;
}

void PGrnIndexColumnSetSourceIDs(Relation index,
								 grn_obj *indexColumn,
								 grn_obj *sourceIDs);

static inline void
PGrnRenameTableRawWithSize(grn_obj *table,
						   const char *newName,
						   size_t newNameSize)
{
	grn_table_rename(ctx, table, newName, newNameSize);
	PGrnCheck("failed to rename table: <%s> -> <%.*s>",
			  PGrnInspectName(table),
			  (int) newNameSize,
			  newName);
}

void PGrnRenameTable(Relation index, grn_obj *table, const char *newName);

void PGrnRemoveObject(const char *name);
void PGrnRemoveObjectWithSize(const char *name, size_t nameSize);
void PGrnRemoveObjectForce(const char *name);
void PGrnRemoveObjectForceWithSize(const char *name, size_t nameSize);

void PGrnRemoveColumns(grn_obj *table);

void PGrnFlushObject(grn_obj *object, bool recursive);

grn_id PGrnPGTypeToGrnType(Oid pgTypeID, unsigned char *flags);
Oid PGrnGrnTypeToPGType(grn_id typeID);
