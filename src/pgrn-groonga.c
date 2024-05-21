#include "pgroonga.h"

#include "pgrn-column-name.h"
#include "pgrn-convert.h"
#include "pgrn-global.h"
#include "pgrn-groonga.h"
#include "pgrn-pg.h"
#include "pgrn-wal.h"

#include <catalog/catalog.h>
#include <catalog/pg_type.h>
#include <miscadmin.h>

bool PGrnIsLZ4Available;
bool PGrnIsZlibAvailable;
bool PGrnIsZstdAvailable;
bool PGrnIsTemporaryIndexSearchAvailable;

static struct PGrnBuffers *buffers = &PGrnBuffers;

static bool
IsTemporaryIndexSearchAvailable(void)
{
	const char *libgroonga_version;
	int major_version;

	libgroonga_version = grn_get_version();

	major_version = atoi(libgroonga_version);

	if (major_version > 8)
		return true;

	if (major_version < 8)
		return false;

	return strcmp(libgroonga_version, "8.0.2") > 0;
}

void
PGrnInitializeGroongaInformation(void)
{
	grn_obj grnIsSupported;

	GRN_BOOL_INIT(&grnIsSupported, 0);

	GRN_BULK_REWIND(&grnIsSupported);
	grn_obj_get_info(ctx, NULL, GRN_INFO_SUPPORT_LZ4, &grnIsSupported);
	PGrnIsLZ4Available = (GRN_BOOL_VALUE(&grnIsSupported));

	GRN_BULK_REWIND(&grnIsSupported);
	grn_obj_get_info(ctx, NULL, GRN_INFO_SUPPORT_ZLIB, &grnIsSupported);
	PGrnIsZlibAvailable = (GRN_BOOL_VALUE(&grnIsSupported));

	GRN_BULK_REWIND(&grnIsSupported);
	grn_obj_get_info(ctx, NULL, GRN_INFO_SUPPORT_ZSTD, &grnIsSupported);
	PGrnIsZstdAvailable = (GRN_BOOL_VALUE(&grnIsSupported));
	PGrnIsTemporaryIndexSearchAvailable = IsTemporaryIndexSearchAvailable();

	GRN_OBJ_FIN(ctx, &grnIsSupported);
}

grn_obj *
PGrnLookup(const char *name, int errorLevel)
{
	return PGrnLookupWithSize(name, strlen(name), errorLevel);
}

grn_obj *
PGrnLookupColumn(grn_obj *table, const char *name, int errorLevel)
{
	return PGrnLookupColumnWithSize(table, name, strlen(name), errorLevel);
}

grn_obj *
PGrnLookupSourcesTable(Relation index, int errorLevel)
{
	char name[GRN_TABLE_MAX_KEY_SIZE];

	snprintf(name,
			 sizeof(name),
			 PGrnSourcesTableNameFormat,
			 PGRN_RELATION_GET_LOCATOR_NUMBER(index));
	return PGrnLookup(name, errorLevel);
}

grn_obj *
PGrnLookupSourcesCtidColumn(Relation index, int errorLevel)
{
	char name[GRN_TABLE_MAX_KEY_SIZE];

	snprintf(name,
			 sizeof(name),
			 PGrnSourcesTableNameFormat "." PGrnSourcesCtidColumnName,
			 PGRN_RELATION_GET_LOCATOR_NUMBER(index));
	return PGrnLookup(name, errorLevel);
}

grn_obj *
PGrnLookupLexicon(Relation index, unsigned int nthAttribute, int errorLevel)
{
	char name[GRN_TABLE_MAX_KEY_SIZE];

	snprintf(name,
			 sizeof(name),
			 PGrnLexiconNameFormat,
			 PGRN_RELATION_GET_LOCATOR_NUMBER(index),
			 nthAttribute);
	return PGrnLookup(name, errorLevel);
}

grn_obj *
PGrnLookupIndexColumn(Relation index, unsigned int nthAttribute, int errorLevel)
{
	char name[GRN_TABLE_MAX_KEY_SIZE];

	snprintf(name,
			 sizeof(name),
			 PGrnLexiconNameFormat ".%s",
			 PGRN_RELATION_GET_LOCATOR_NUMBER(index),
			 nthAttribute,
			 PGrnIndexColumnName);
	return PGrnLookup(name, errorLevel);
}

void
PGrnFormatSourcesTableName(const char *indexName,
						   char output[GRN_TABLE_MAX_KEY_SIZE])
{
	Oid indexID;
	Oid fileNodeID;
	indexID = PGrnPGIndexNameToID(indexName);
	fileNodeID = PGrnPGIndexIDToFileNodeID(indexID);
	snprintf(
		output, GRN_TABLE_MAX_KEY_SIZE, PGrnSourcesTableNameFormat, fileNodeID);
}

grn_obj *
PGrnCreateTable(Relation index,
				const char *name,
				grn_table_flags flags,
				grn_obj *type,
				grn_obj *tokenizer,
				grn_obj *normalizers,
				grn_obj *tokenFilters)
{
	unsigned int nameSize = 0;

	if (name)
		nameSize = strlen(name);

	return PGrnCreateTableWithSize(index,
								   name,
								   nameSize,
								   flags,
								   type,
								   tokenizer,
								   normalizers,
								   tokenFilters);
}

grn_obj *
PGrnCreateTableWithSize(Relation index,
						const char *name,
						size_t nameSize,
						grn_table_flags flags,
						grn_obj *type,
						grn_obj *tokenizer,
						grn_obj *normalizers,
						grn_obj *tokenFilters)
{
	Oid indexTableSpaceID = InvalidOid;
	grn_obj *table;

	if (name)
	{
		flags |= GRN_OBJ_PERSISTENT;
	}

	if (RelationIsValid(index))
	{
		indexTableSpaceID = PGRN_RELATION_GET_LOCATOR_SPACE(index);
		if (indexTableSpaceID == MyDatabaseTableSpace)
			indexTableSpaceID = InvalidOid;
	}

	table = PGrnCreateTableRawWithSize(indexTableSpaceID,
									   name,
									   nameSize,
									   flags,
									   type,
									   tokenizer,
									   normalizers,
									   tokenFilters);

	PGrnWALCreateTable(index,
					   name,
					   nameSize,
					   flags,
					   type,
					   tokenizer,
					   normalizers,
					   tokenFilters);

	return table;
}

grn_obj *
PGrnCreateSimilarTemporaryLexicon(Relation index,
								  const char *attributeName,
								  size_t attributeNameSize,
								  const char *tag)
{
	grn_obj *lexicon = NULL;
	grn_table_flags flags = 0;
	grn_obj *keyType = NULL;
	grn_obj *tokenizer = NULL;
	grn_obj *tokenizerBuffer = &(buffers->tokenizer);
	grn_obj *normalizers = NULL;
	grn_obj *normalizersBuffer = &(buffers->normalizers);
	grn_obj *tokenFilters = NULL;
	grn_obj *tokenFiltersBuffer = &(buffers->tokenFilters);
	grn_obj *temporaryLexicon = NULL;

	if (PGrnPGIsParentIndex(index))
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("pgroonga: %s PGrnCreateSimilarTemporaryLexicon: "
						"index must not a parent index: <%s%s%.*s>",
						tag,
						RelationGetRelationName(index),
						attributeNameSize > 0 ? "." : "",
						attributeNameSize > 0 ? (int) attributeNameSize : 0,
						attributeName)));
	}

	if (attributeNameSize > 0)
	{
		int i = PGrnPGResolveAttributeIndex(
			index, attributeName, attributeNameSize);
		if (i != -1)
		{
			lexicon = PGrnLookupLexicon(index, i, ERROR);
			if (!grn_type_id_is_text_family(ctx, lexicon->header.domain))
			{
				grn_obj_unref(ctx, lexicon);
				lexicon = NULL;
			}
		}
	}
	else
	{
		int i;
		for (i = 0; i < index->rd_att->natts; i++)
		{
			lexicon = PGrnLookupLexicon(index, i, ERROR);
			if (grn_type_id_is_text_family(ctx, lexicon->header.domain))
			{
				break;
			}
			grn_obj_unref(ctx, lexicon);
			lexicon = NULL;
		}
	}
	if (!lexicon)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("pgroonga: %s PGrnCreateSimilarTemporaryLexicon: "
						"index doesn't have a lexicon for text: "
						"<%s%s%.*s>",
						tag,
						RelationGetRelationName(index),
						attributeNameSize > 0 ? "." : "",
						attributeNameSize > 0 ? (int) attributeNameSize : 0,
						attributeName)));
	}

	switch (lexicon->header.type)
	{
	case GRN_TABLE_HASH_KEY:
		flags |= GRN_OBJ_TABLE_HASH_KEY;
		break;
	case GRN_TABLE_PAT_KEY:
		flags |= GRN_OBJ_TABLE_PAT_KEY;
		break;
	case GRN_TABLE_DAT_KEY:
		flags |= GRN_OBJ_TABLE_DAT_KEY;
		break;
	default:
		break;
	}

	keyType = grn_ctx_at(ctx, lexicon->header.domain);

	GRN_BULK_REWIND(tokenizerBuffer);
	grn_table_get_default_tokenizer_string(ctx, lexicon, tokenizerBuffer);
	if (GRN_TEXT_LEN(tokenizerBuffer) > 0)
	{
		tokenizer = tokenizerBuffer;
	}

	GRN_BULK_REWIND(normalizersBuffer);
	grn_table_get_normalizers_string(ctx, lexicon, normalizersBuffer);
	if (GRN_TEXT_LEN(normalizersBuffer) > 0)
	{
		normalizers = normalizersBuffer;
	}

	GRN_BULK_REWIND(tokenFiltersBuffer);
	grn_table_get_token_filters_string(ctx, lexicon, tokenFiltersBuffer);
	if (GRN_TEXT_LEN(tokenFiltersBuffer) > 0)
	{
		tokenFilters = tokenFiltersBuffer;
	}

	temporaryLexicon = PGrnCreateTable(
		index, NULL, flags, keyType, tokenizer, normalizers, tokenFilters);

	grn_obj_unref(ctx, lexicon);
	grn_obj_unref(ctx, keyType);

	return temporaryLexicon;
}

grn_obj *
PGrnCreateColumn(Relation index,
				 grn_obj *table,
				 const char *name,
				 grn_column_flags flags,
				 grn_obj *type)
{
	return PGrnCreateColumnWithSize(
		index, table, name, strlen(name), flags, type);
}

grn_obj *
PGrnCreateColumnWithSize(Relation index,
						 grn_obj *table,
						 const char *name,
						 size_t nameSize,
						 grn_column_flags flags,
						 grn_obj *type)
{
	Oid indexTableSpaceID = InvalidOid;
	grn_obj *column;

	if (name)
	{
		flags |= GRN_OBJ_PERSISTENT;
	}

	if (RelationIsValid(index))
	{
		indexTableSpaceID = PGRN_RELATION_GET_LOCATOR_SPACE(index);
		if (indexTableSpaceID == MyDatabaseTableSpace)
			indexTableSpaceID = InvalidOid;
	}

	column = PGrnCreateColumnRawWithSize(
		indexTableSpaceID, table, name, nameSize, flags, type);

	PGrnWALCreateColumn(index, table, name, nameSize, flags, type);

	return column;
}

void
PGrnIndexColumnClearSources(Relation index, grn_obj *indexColumn)
{
	GRN_BULK_REWIND(&(buffers->sourceIDs));
	PGrnIndexColumnSetSourceIDs(index, indexColumn, &(buffers->sourceIDs));
}

void
PGrnIndexColumnSetSource(Relation index, grn_obj *indexColumn, grn_obj *source)
{
	grn_id sourceID;

	GRN_BULK_REWIND(&(buffers->sourceIDs));

	sourceID = grn_obj_id(ctx, source);
	GRN_UINT32_PUT(ctx, &(buffers->sourceIDs), sourceID);

	PGrnIndexColumnSetSourceIDs(index, indexColumn, &(buffers->sourceIDs));
}

void
PGrnIndexColumnSetSourceIDs(Relation index,
							grn_obj *indexColumn,
							grn_obj *sourceIDs)
{
	if (!PGrnIndexColumnSetSourceIDsRaw(indexColumn, sourceIDs))
		return;

	PGrnWALSetSourceIDs(index, indexColumn, sourceIDs);
}

void
PGrnRenameTable(Relation index, grn_obj *table, const char *newName)
{
	char name[GRN_TABLE_MAX_KEY_SIZE];
	int nameSize;
	size_t newNameSize;

	nameSize = grn_obj_name(ctx, table, name, GRN_TABLE_MAX_KEY_SIZE);
	newNameSize = strlen(newName);
	PGrnRenameTableRawWithSize(table, newName, newNameSize);

	PGrnWALRenameTable(index, name, nameSize, newName, newNameSize);
}

void
PGrnRemoveObject(const char *name)
{
	PGrnRemoveObjectWithSize(name, strlen(name));
}

void
PGrnRemoveObjectWithSize(const char *name, size_t nameSize)
{
	grn_obj *object;

	object = PGrnLookupWithSize(name, nameSize, ERROR);
	grn_obj_remove(ctx, object);
	PGrnCheck("failed to remove: <%.*s>", (int) nameSize, name);
}

void
PGrnRemoveObjectForce(Relation index, const char *name)
{
	PGrnRemoveObjectForceWithSize(index, name, strlen(name));
}

void
PGrnRemoveObjectForceWithSize(Relation index, const char *name, size_t nameSize)
{
	grn_obj *object;

	object = PGrnLookupWithSize(name, nameSize, PGRN_ERROR_LEVEL_IGNORE);
	if (object)
	{
		if (grn_obj_remove(ctx, object) != GRN_SUCCESS)
		{
			object = NULL;
		}
	}
	if (!object)
	{
		grn_obj_remove_force(ctx, name, nameSize);
	}
	PGrnCheck("failed to remove: <%.*s>", (int) nameSize, name);

	PGrnWALRemoveObject(index, name, nameSize);
}

void
PGrnFlushObject(grn_obj *object, bool recursive)
{
	grn_rc rc;
	char name[GRN_TABLE_MAX_KEY_SIZE];
	int nameSize;

	if (recursive)
	{
		rc = grn_obj_flush_recursive(ctx, object);
	}
	else
	{
		rc = grn_obj_flush(ctx, object);
	}
	if (rc == GRN_SUCCESS)
		return;

	nameSize = grn_obj_name(ctx, object, name, GRN_TABLE_MAX_KEY_SIZE);
	PGrnCheck("failed to flush: <%.*s>", nameSize, name);
}

void
PGrnRemoveColumns(Relation index, grn_obj *table)
{
	grn_hash *columns;

	columns = grn_hash_create(
		ctx, NULL, sizeof(grn_id), 0, GRN_TABLE_HASH_KEY | GRN_HASH_TINY);
	if (!columns)
	{
		PGrnCheck("failed to create columns container for removing columns: "
				  "<%s>",
				  PGrnInspectName(table));
	}
	grn_table_columns(ctx, table, "", 0, (grn_obj *) columns);
	PGrnCheck("failed to collect columns for removing columns: <%s>",
			  PGrnInspectName(table));

	GRN_HASH_EACH_BEGIN(ctx, columns, cursor, id)
	{
		grn_id *columnID;
		grn_obj *column;
		char columnName[GRN_TABLE_MAX_KEY_SIZE];
		int columnNameSize;

		grn_hash_cursor_get_key(ctx, cursor, (void **) &columnID);
		column = grn_ctx_at(ctx, *columnID);
		if (!column)
			continue;

		columnNameSize =
			grn_obj_name(ctx, column, columnName, GRN_TABLE_MAX_KEY_SIZE);
		grn_obj_remove(ctx, column);
		PGrnCheck(
			"failed to remove column: <%.*s>", columnNameSize, columnName);

		PGrnWALRemoveObject(index, columnName, columnNameSize);
	}
	GRN_HASH_EACH_END(ctx, cursor);

	grn_hash_close(ctx, columns);
}

grn_id
PGrnPGTypeToGrnType(Oid pgTypeID, unsigned char *flags)
{
	const char *tag = "[type][postgresql->groonga]";
	grn_id typeID = GRN_ID_NIL;
	unsigned char typeFlags = 0;

	switch (pgTypeID)
	{
	case BOOLOID:
		typeID = GRN_DB_BOOL;
		break;
	case INT2OID:
		typeID = GRN_DB_INT16;
		break;
	case INT4OID:
		typeID = GRN_DB_INT32;
		break;
	case INT8OID:
		typeID = GRN_DB_INT64;
		break;
	case FLOAT4OID:
		typeID = GRN_DB_FLOAT32;
		break;
	case FLOAT8OID:
		typeID = GRN_DB_FLOAT;
		break;
	case TIMESTAMPOID:
	case TIMESTAMPTZOID:
		typeID = GRN_DB_TIME;
		break;
	case TEXTOID:
	case XMLOID:
		typeID = GRN_DB_LONG_TEXT;
		break;
	case VARCHAROID:
		typeID = GRN_DB_SHORT_TEXT; /* 4KB */
		break;
#ifdef NOT_USED
	case POINTOID:
		typeID = GRN_DB_TOKYO_GEO_POINT or GRN_DB_WGS84_GEO_POINT;
		break;
#endif
	case INT4ARRAYOID:
		typeID = GRN_DB_INT32;
		typeFlags |= GRN_OBJ_VECTOR;
		break;
	case VARCHARARRAYOID:
		typeID = GRN_DB_SHORT_TEXT;
		typeFlags |= GRN_OBJ_VECTOR;
		break;
	case TEXTARRAYOID:
		typeID = GRN_DB_LONG_TEXT;
		typeFlags |= GRN_OBJ_VECTOR;
		break;
	case UUIDOID:
		typeID = GRN_DB_SHORT_TEXT;
		break;
	default:
		PGrnCheckRC(GRN_FUNCTION_NOT_IMPLEMENTED,
					"%s unsupported type: %u",
					tag,
					pgTypeID);
		break;
	}

	if (flags)
	{
		*flags = typeFlags;
	}

	return typeID;
}

/* TODO: Support vector */
Oid
PGrnGrnTypeToPGType(grn_id typeID)
{
	const char *tag = "[type][groonga->postgresql]";
	Oid pgTypeID = InvalidOid;

	switch (typeID)
	{
	case GRN_DB_BOOL:
		pgTypeID = BOOLOID;
		break;
	case GRN_DB_INT8:
	case GRN_DB_UINT8:
	case GRN_DB_INT16:
		pgTypeID = INT2OID;
		break;
	case GRN_DB_UINT16:
	case GRN_DB_INT32:
		pgTypeID = INT4OID;
		break;
	case GRN_DB_UINT32:
	case GRN_DB_INT64:
	case GRN_DB_UINT64:
		pgTypeID = INT8OID;
		break;
	case GRN_DB_FLOAT32:
		pgTypeID = FLOAT4OID;
		break;
	case GRN_DB_FLOAT:
		pgTypeID = FLOAT8OID;
		break;
	case GRN_DB_TIME:
		pgTypeID = TIMESTAMPOID;
		break;
	case GRN_DB_SHORT_TEXT:
	case GRN_DB_TEXT:
	case GRN_DB_LONG_TEXT:
		pgTypeID = TEXTOID;
		break;
	default:
		if (grn_id_maybe_table(ctx, typeID))
		{
			grn_obj *table = grn_ctx_at(ctx, typeID);
			grn_id keyTypeID = GRN_ID_NIL;
			if (grn_obj_is_table_with_key(ctx, table))
			{
				keyTypeID = table->header.domain;
			}
			grn_obj_unref(ctx, table);
			if (keyTypeID != GRN_ID_NIL)
				return PGrnGrnTypeToPGType(keyTypeID);
		}
		PGrnCheckRC(GRN_FUNCTION_NOT_IMPLEMENTED,
					"%s unsupported type: %d",
					tag,
					typeID);
		break;
	}

	return pgTypeID;
}

void
PGrnExprAppendOp(grn_obj *expr, grn_operator op, int nArgs, const char *tag)
{
	grn_expr_append_op(ctx, expr, op, nArgs);
	PGrnCheck("%s: failed to append operator: %s(%d)",
			  tag,
			  grn_operator_to_string(op),
			  nArgs);
}
