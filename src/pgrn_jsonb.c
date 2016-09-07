#include "pgroonga.h"

#include "pgrn_compatible.h"
#include "pgrn_convert.h"
#include "pgrn_global.h"
#include "pgrn_groonga.h"
#include "pgrn_jsonb.h"
#include "pgrn_options.h"
#include "pgrn_value.h"

#include <catalog/pg_type.h>
#include <utils/builtins.h>
#ifdef PGRN_SUPPORT_JSONB
#	include <utils/jsonb.h>
#endif

#include <groonga.h>

#include <xxhash.h>

#ifdef PGRN_SUPPORT_JSONB
PG_FUNCTION_INFO_V1(pgroonga_match_jsonb);
#endif

#ifdef PGRN_SUPPORT_JSONB
typedef struct
{
	grn_obj *pathsTable;
	grn_obj *typesTable;
	grn_obj *valuesTable;
} PGrnJSONBCreateData;

typedef struct PGrnJSONBInsertData
{
	grn_obj *pathsTable;
	grn_obj *valuesTable;
	grn_obj *pathColumn;
	grn_obj *pathsColumn;
	grn_obj *stringColumn;
	grn_obj *numberColumn;
	grn_obj *booleanColumn;
	grn_obj *sizeColumn;
	grn_obj *typeColumn;
	grn_obj *valueIDs;
	grn_obj key;
	grn_obj components;
	grn_obj path;
	grn_obj pathIDs;
	grn_obj value;
	grn_obj type;
} PGrnJSONBInsertData;

static grn_ctx *ctx = &PGrnContext;
static struct PGrnBuffers *buffers = &PGrnBuffers;

static const unsigned int PGRN_JSON_GENERATE_PATH_IS_ABSOLUTE   = 1 << 0;
static const unsigned int PGRN_JSON_GENERATE_PATH_INCLUDE_ARRAY = 1 << 1;
static const unsigned int PGRN_JSON_GENERATE_PATH_USE_DOT_STYLE = 1 << 2;

static grn_obj *tmpPathsTable = NULL;
static grn_obj *tmpTypesTable = NULL;
static grn_obj *tmpValuesTable = NULL;

grn_obj *
PGrnJSONBLookupValuesTable(Relation index,
						   unsigned int nthAttribute,
						   int errorLevel)
{
	char name[GRN_TABLE_MAX_KEY_SIZE];

	snprintf(name, sizeof(name),
			 PGrnJSONValuesTableNameFormat,
			 index->rd_node.relNode, nthAttribute);
	return PGrnLookup(name, errorLevel);
}

grn_obj *
PGrnJSONBLookupPathsTable(Relation index,
						  unsigned int nthAttribute,
						  int errorLevel)
{
	char name[GRN_TABLE_MAX_KEY_SIZE];

	snprintf(name, sizeof(name),
			 PGrnJSONPathsTableNameFormat,
			 index->rd_node.relNode, nthAttribute);
	return PGrnLookup(name, errorLevel);
}

grn_obj *
PGrnJSONBLookupTypesTable(Relation index,
						  unsigned int nthAttribute,
						  int errorLevel)
{
	char name[GRN_TABLE_MAX_KEY_SIZE];

	snprintf(name, sizeof(name),
			 PGrnJSONTypesTableNameFormat,
			 index->rd_node.relNode, nthAttribute);
	return PGrnLookup(name, errorLevel);
}

grn_obj *
PGrnJSONBLookupFullTextSearchLexicon(Relation index,
									 unsigned int nthAttribute,
									 int errorLevel)
{
	char name[GRN_TABLE_MAX_KEY_SIZE];

	snprintf(name, sizeof(name),
			 PGrnJSONValueLexiconNameFormat,
			 "FullTextSearch", index->rd_node.relNode, nthAttribute);
	return PGrnLookup(name, errorLevel);
}

grn_obj *
PGrnJSONBLookupStringLexicon(Relation index,
							 unsigned int nthAttribute,
							 int errorLevel)
{
	char name[GRN_TABLE_MAX_KEY_SIZE];

	snprintf(name, sizeof(name),
			 PGrnJSONValueLexiconNameFormat,
			 "String", index->rd_node.relNode, nthAttribute);
	return PGrnLookup(name, errorLevel);
}

grn_obj *
PGrnJSONBLookupNumberLexicon(Relation index,
							 unsigned int nthAttribute,
							 int errorLevel)
{
	char name[GRN_TABLE_MAX_KEY_SIZE];

	snprintf(name, sizeof(name),
			 PGrnJSONValueLexiconNameFormat,
			 "Number", index->rd_node.relNode, nthAttribute);
	return PGrnLookup(name, errorLevel);
}

grn_obj *
PGrnJSONBLookupBooleanLexicon(Relation index,
							  unsigned int nthAttribute,
							  int errorLevel)
{
	char name[GRN_TABLE_MAX_KEY_SIZE];

	snprintf(name, sizeof(name),
			 PGrnJSONValueLexiconNameFormat,
			 "Boolean", index->rd_node.relNode, nthAttribute);
	return PGrnLookup(name, errorLevel);
}

grn_obj *
PGrnJSONBLookupSizeLexicon(Relation index,
						   unsigned int nthAttribute,
						   int errorLevel)
{
	char name[GRN_TABLE_MAX_KEY_SIZE];

	snprintf(name, sizeof(name),
			 PGrnJSONValueLexiconNameFormat,
			 "Size", index->rd_node.relNode, nthAttribute);
	return PGrnLookup(name, errorLevel);
}

static grn_obj *
PGrnJSONBCreatePathsTable(const char *name)
{
	return PGrnCreateTable(name,
						   GRN_OBJ_TABLE_PAT_KEY,
						   grn_ctx_at(ctx, GRN_DB_SHORT_TEXT));
}

static grn_obj *
PGrnJSONBCreateTypesTable(const char *name)
{
	return PGrnCreateTable(name,
						   GRN_OBJ_TABLE_PAT_KEY,
						   grn_ctx_at(ctx, GRN_DB_SHORT_TEXT));
}

static grn_obj *
PGrnJSONBCreateValuesTable(const char *name)
{
	return PGrnCreateTable(name,
						   GRN_OBJ_TABLE_HASH_KEY,
						   grn_ctx_at(ctx, GRN_DB_UINT64));
}

static void
PGrnJSONBCreateDataColumns(PGrnJSONBCreateData *jsonbData)
{
	PGrnCreateColumn(jsonbData->valuesTable,
					 "path",
					 GRN_OBJ_COLUMN_SCALAR,
					 jsonbData->pathsTable);
	PGrnCreateColumn(jsonbData->valuesTable,
					 "paths",
					 GRN_OBJ_COLUMN_VECTOR,
					 jsonbData->pathsTable);
	{
		grn_obj_flags flags = 0;
		if (PGrnIsLZ4Available)
			flags |= GRN_OBJ_COMPRESS_LZ4;
		PGrnCreateColumn(jsonbData->valuesTable,
						 "string",
						 flags,
						 grn_ctx_at(ctx, GRN_DB_LONG_TEXT));
	}
	PGrnCreateColumn(jsonbData->valuesTable,
					 "number",
					 0,
					 grn_ctx_at(ctx, GRN_DB_FLOAT));
	PGrnCreateColumn(jsonbData->valuesTable,
					 "boolean",
					 0,
					 grn_ctx_at(ctx, GRN_DB_BOOL));
	PGrnCreateColumn(jsonbData->valuesTable,
					 "size",
					 0,
					 grn_ctx_at(ctx, GRN_DB_UINT32));
	PGrnCreateColumn(jsonbData->valuesTable,
					 "type",
					 0,
					 jsonbData->typesTable);
}

static void
PGrnJSONGeneratePath(grn_obj *components,
					 unsigned int start,
					 unsigned int flags,
					 grn_obj *path)
{
	unsigned int i, n;
	unsigned int minimumPathSize = 0;

	n = grn_vector_size(ctx, components);

	if (flags & PGRN_JSON_GENERATE_PATH_IS_ABSOLUTE)
	{
		GRN_TEXT_PUTS(ctx, path, ".");
		minimumPathSize = 1;
	}

	for (i = start; i < n; i++)
	{
		const char *component;
		unsigned int componentSize;
		grn_id domain;

		componentSize = grn_vector_get_element(ctx,
											   components,
											   i,
											   &component,
											   NULL,
											   &domain);
		if (domain == GRN_DB_UINT32)
		{
			if (flags & PGRN_JSON_GENERATE_PATH_INCLUDE_ARRAY)
				GRN_TEXT_PUTS(ctx, path, "[]");
		}
		else
		{
			if (flags & PGRN_JSON_GENERATE_PATH_USE_DOT_STYLE)
			{
				if (GRN_TEXT_LEN(path) > minimumPathSize)
					GRN_TEXT_PUTS(ctx, path, ".");
				GRN_TEXT_PUT(ctx, path, component, componentSize);
			}
			else
			{
				GRN_TEXT_PUTS(ctx, path, "[");
				grn_text_esc(ctx, path, component, componentSize);
				GRN_TEXT_PUTS(ctx, path, "]");
			}
		}
	}
}

static void
PGrnJSONGenerateCompletePath(grn_obj *components, grn_obj *path)
{
	PGrnJSONGeneratePath(components,
						 0,
						 PGRN_JSON_GENERATE_PATH_IS_ABSOLUTE |
						 PGRN_JSON_GENERATE_PATH_INCLUDE_ARRAY,
						 path);
}

static const char *
PGrnJSONIteratorTokenName(JsonbIteratorToken token)
{
	static char *names[] = {
		"WJB_DONE",
		"WJB_KEY",
		"WJB_VALUE",
		"WJB_ELEM",
		"WJB_BEGIN_ARRAY",
		"WJB_END_ARRAY",
		"WJB_BEGIN_OBJECT",
		"WJB_END_OBJECT"
	};

	return names[token];
}

static void
PGrnJSONBInsertDataInit(PGrnJSONBInsertData *data)
{
	data->pathColumn =
		PGrnLookupColumn(data->valuesTable, "path", ERROR);
	data->pathsColumn =
		PGrnLookupColumn(data->valuesTable, "paths", ERROR);
	data->stringColumn =
		PGrnLookupColumn(data->valuesTable, "string", ERROR);
	data->numberColumn =
		PGrnLookupColumn(data->valuesTable, "number", ERROR);
	data->booleanColumn =
		PGrnLookupColumn(data->valuesTable, "boolean", ERROR);
	data->sizeColumn =
		PGrnLookupColumn(data->valuesTable, "size", ERROR);
	data->typeColumn =
		PGrnLookupColumn(data->valuesTable, "type", ERROR);

	GRN_TEXT_INIT(&(data->key), 0);
	GRN_TEXT_INIT(&(data->components), GRN_OBJ_VECTOR);
	GRN_TEXT_INIT(&(data->path), 0);
	GRN_RECORD_INIT(&(data->pathIDs), GRN_OBJ_VECTOR,
					grn_obj_id(ctx, data->pathsTable));
	GRN_VOID_INIT(&(data->value));
	GRN_TEXT_INIT(&(data->type), GRN_OBJ_DO_SHALLOW_COPY);
}

static void
PGrnJSONBInsertDataFin(PGrnJSONBInsertData *data)
{
	GRN_OBJ_FIN(ctx, &(data->type));
	GRN_OBJ_FIN(ctx, &(data->value));
	GRN_OBJ_FIN(ctx, &(data->pathIDs));
	GRN_OBJ_FIN(ctx, &(data->path));
	GRN_OBJ_FIN(ctx, &(data->components));
	GRN_OBJ_FIN(ctx, &(data->key));
}

static uint64_t
PGrnJSONBInsertGenerateKey(PGrnJSONBInsertData *data,
						   bool haveValue,
						   const char *typeName)
{
	unsigned int i, n;

	GRN_BULK_REWIND(&(data->key));

	GRN_TEXT_PUTS(ctx, &(data->key), ".");
	n = grn_vector_size(ctx, &(data->components));
	for (i = 0; i < n; i++)
	{
		const char *component;
		unsigned int componentSize;
		grn_id domain;

		componentSize = grn_vector_get_element(ctx,
											   &(data->components),
											   i,
											   &component,
											   NULL,
											   &domain);
		if (domain == GRN_DB_UINT32)
		{
			GRN_TEXT_PUTS(ctx, &(data->key), "[]");
		}
		else
		{
			GRN_TEXT_PUTS(ctx, &(data->key), "[");
			grn_text_esc(ctx, &(data->key), component, componentSize);
			GRN_TEXT_PUTS(ctx, &(data->key), "]");
		}
	}

	GRN_TEXT_PUTS(ctx, &(data->key), "|");
	GRN_TEXT_PUTS(ctx, &(data->key), typeName);

	if (haveValue)
	{
		GRN_TEXT_PUTS(ctx, &(data->key), "|");
		grn_obj_cast(ctx, &(data->value), &(data->key), GRN_FALSE);
	}

	return XXH64(GRN_TEXT_VALUE(&data->key),
				 GRN_TEXT_LEN(&data->key),
				 0);
}

static void
PGrnJSONBInsertAddPath(PGrnJSONBInsertData *data,
					   unsigned int start,
					   unsigned int flags)
{
	grn_id pathID;

	GRN_BULK_REWIND(&(data->path));
	PGrnJSONGeneratePath(&(data->components),
						 start,
						 flags,
						 &(data->path));

	if (GRN_TEXT_LEN(&(data->path)) >= GRN_TABLE_MAX_KEY_SIZE)
		return;

	pathID = grn_table_add(ctx, data->pathsTable,
						   GRN_TEXT_VALUE(&(data->path)),
						   GRN_TEXT_LEN(&(data->path)),
						   NULL);
	if (pathID == GRN_ID_NIL)
		return;

	{
		unsigned int i, n;

		n = GRN_BULK_VSIZE(&(data->pathIDs)) / sizeof(grn_id);
		for (i = 0; i < n; i++)
		{
			if (GRN_RECORD_VALUE_AT(&(data->pathIDs), i) == pathID)
				return;
		}
	}

	GRN_RECORD_PUT(ctx, &(data->pathIDs), pathID);
}

static void
PGrnJSONBInsertGenerateSubPathsRecursive(PGrnJSONBInsertData *data,
										 unsigned int parentStart)
{
	if (parentStart == grn_vector_size(ctx, &(data->components)))
		return;

	PGrnJSONBInsertAddPath(data,
						   parentStart,
						   PGRN_JSON_GENERATE_PATH_USE_DOT_STYLE);
	PGrnJSONBInsertAddPath(data,
						   parentStart,
						   0);
	PGrnJSONBInsertAddPath(data,
						   parentStart,
						   PGRN_JSON_GENERATE_PATH_INCLUDE_ARRAY);

	PGrnJSONBInsertGenerateSubPathsRecursive(data, parentStart + 1);
}

static void
PGrnJSONBInsertGeneratePaths(PGrnJSONBInsertData *data)
{
	GRN_BULK_REWIND(&(data->pathIDs));

	PGrnJSONBInsertAddPath(data,
						   0,
						   PGRN_JSON_GENERATE_PATH_IS_ABSOLUTE |
						   PGRN_JSON_GENERATE_PATH_USE_DOT_STYLE);
	PGrnJSONBInsertAddPath(data,
						   0,
						   PGRN_JSON_GENERATE_PATH_IS_ABSOLUTE);
	PGrnJSONBInsertAddPath(data,
						   0,
						   PGRN_JSON_GENERATE_PATH_IS_ABSOLUTE |
						   PGRN_JSON_GENERATE_PATH_INCLUDE_ARRAY);

	PGrnJSONBInsertGenerateSubPathsRecursive(data, 0);
}

static void
PGrnJSONBInsertValueSet(PGrnJSONBInsertData *data,
						grn_obj *column,
						const char *typeName)
{
	uint64_t key;
	grn_id valueID;
	int added;

	key = PGrnJSONBInsertGenerateKey(data, column != NULL, typeName);
	valueID = grn_table_add(ctx, data->valuesTable,
							&key, sizeof(uint64_t),
							&added);
	GRN_RECORD_PUT(ctx, data->valueIDs, valueID);
	if (!added)
		return;

	GRN_BULK_REWIND(&(data->path));
	PGrnJSONGenerateCompletePath(&(data->components), &(data->path));
	if (GRN_TEXT_LEN(&(data->path)) < GRN_TABLE_MAX_KEY_SIZE)
		grn_obj_set_value(ctx, data->pathColumn, valueID,
						  &(data->path), GRN_OBJ_SET);

	PGrnJSONBInsertGeneratePaths(data);
	grn_obj_set_value(ctx, data->pathsColumn, valueID,
					  &(data->pathIDs), GRN_OBJ_SET);

	if (column)
		grn_obj_set_value(ctx, column, valueID, &(data->value), GRN_OBJ_SET);

	GRN_TEXT_SETS(ctx, &(data->type), typeName);
	grn_obj_set_value(ctx, data->typeColumn, valueID,
					  &(data->type), GRN_OBJ_SET);
}

static void PGrnJSONBInsertContainer(JsonbIterator **iter,
									 PGrnJSONBInsertData *data);

static void
PGrnJSONBInsertValue(JsonbIterator **iter,
					 JsonbValue *value,
					 PGrnJSONBInsertData *data)
{
	switch (value->type)
	{
	case jbvNull:
		PGrnJSONBInsertValueSet(data, NULL, "null");
		break;
	case jbvString:
		grn_obj_reinit(ctx, &(data->value), GRN_DB_LONG_TEXT,
					   GRN_OBJ_DO_SHALLOW_COPY);
		GRN_TEXT_SET(ctx, &(data->value),
					 value->val.string.val,
					 value->val.string.len);
		PGrnJSONBInsertValueSet(data, data->stringColumn, "string");
		break;
	case jbvNumeric:
	{
		Datum numericInString =
			DirectFunctionCall1(numeric_out,
								NumericGetDatum(value->val.numeric));
		const char *numericInCString = DatumGetCString(numericInString);
		grn_obj_reinit(ctx, &(data->value), GRN_DB_TEXT,
					   GRN_OBJ_DO_SHALLOW_COPY);
		GRN_TEXT_SETS(ctx, &(data->value), numericInCString);
		PGrnJSONBInsertValueSet(data, data->numberColumn, "number");
		break;
	}
	case jbvBool:
		grn_obj_reinit(ctx, &(data->value), GRN_DB_BOOL, 0);
		GRN_BOOL_SET(ctx, &(data->value), value->val.boolean);
		PGrnJSONBInsertValueSet(data, data->booleanColumn, "boolean");
		break;
	case jbvArray:
		PGrnJSONBInsertContainer(iter, data);
		break;
	case jbvObject:
		PGrnJSONBInsertContainer(iter, data);
		break;
	case jbvBinary:
		PGrnJSONBInsertContainer(iter, data);
		break;
	}
}

static void
PGrnJSONBInsertContainer(JsonbIterator **iter, PGrnJSONBInsertData *data)
{
	JsonbIteratorToken token;
	JsonbValue value;

	while ((token = JsonbIteratorNext(iter, &value, false)) != WJB_DONE) {
		switch (token)
		{
		case WJB_KEY:
			grn_vector_add_element(ctx, &(data->components),
								   value.val.string.val,
								   value.val.string.len,
								   0,
								   GRN_DB_SHORT_TEXT);
			break;
		case WJB_VALUE:
			PGrnJSONBInsertValue(iter, &value, data);
			{
				const char *component;
				grn_vector_pop_element(ctx, &(data->components), &component,
									   NULL, NULL);
			}
			break;
		case WJB_ELEM:
			PGrnJSONBInsertValue(iter, &value, data);
			break;
		case WJB_BEGIN_ARRAY:
		{
			uint32_t nElements = value.val.array.nElems;
			grn_vector_add_element(ctx, &(data->components),
								   (const char *)&nElements,
								   sizeof(uint32_t),
								   0,
								   GRN_DB_UINT32);
			PGrnJSONBInsertValueSet(data, NULL, "array");
			break;
		}
		case WJB_END_ARRAY:
		{
			const char *component;
			grn_vector_pop_element(ctx, &(data->components), &component,
								   NULL, NULL);
			break;
		}
		case WJB_BEGIN_OBJECT:
			PGrnJSONBInsertValueSet(data, NULL, "object");
			break;
		case WJB_END_OBJECT:
			break;
		default:
			ereport(ERROR,
					(errcode(ERRCODE_SYSTEM_ERROR),
					 errmsg("pgroonga: jsonb iterator returns invalid token: %d",
							token)));
			break;
		}
	}
}
#endif

void
PGrnInitializeJSONB(void)
{
#ifdef PGRN_SUPPORT_JSONB
	PGrnJSONBCreateData data;

	tmpPathsTable = PGrnJSONBCreatePathsTable(NULL);
	tmpTypesTable = PGrnJSONBCreateTypesTable(NULL);
	tmpValuesTable = PGrnJSONBCreateValuesTable(NULL);

	data.pathsTable = tmpPathsTable;
	data.typesTable = tmpTypesTable;
	data.valuesTable = tmpValuesTable;
	PGrnJSONBCreateDataColumns(&data);
#endif
}

void
PGrnFinalizeJSONB(void)
{
#ifdef PGRN_SUPPORT_JSONB
	grn_obj_remove(ctx, tmpValuesTable);
	grn_obj_remove(ctx, tmpTypesTable);
	grn_obj_remove(ctx, tmpPathsTable);
#endif
}

#ifdef PGRN_SUPPORT_JSONB
static void
PGrnJSONBCreateTables(PGrnCreateData *data,
					  PGrnJSONBCreateData *jsonbData)
{
	{
		char jsonPathsTableName[GRN_TABLE_MAX_KEY_SIZE];
		snprintf(jsonPathsTableName, sizeof(jsonPathsTableName),
				 PGrnJSONPathsTableNameFormat,
				 data->relNode, data->i);
		jsonbData->pathsTable = PGrnJSONBCreatePathsTable(jsonPathsTableName);
		GRN_PTR_PUT(ctx, data->supplementaryTables, jsonbData->pathsTable);
	}

	{
		char jsonTypesTableName[GRN_TABLE_MAX_KEY_SIZE];
		snprintf(jsonTypesTableName, sizeof(jsonTypesTableName),
				 PGrnJSONTypesTableNameFormat,
				 data->relNode, data->i);
		jsonbData->typesTable = PGrnJSONBCreateTypesTable(jsonTypesTableName);
		GRN_PTR_PUT(ctx, data->supplementaryTables, jsonbData->typesTable);
	}

	{
		char jsonValuesTableName[GRN_TABLE_MAX_KEY_SIZE];
		snprintf(jsonValuesTableName, sizeof(jsonValuesTableName),
				 PGrnJSONValuesTableNameFormat,
				 data->relNode, data->i);
		jsonbData->valuesTable = PGrnJSONBCreateValuesTable(jsonValuesTableName);
		GRN_PTR_PUT(ctx, data->supplementaryTables, jsonbData->valuesTable);
	}
}

static void
PGrnJSONBCreateFullTextSearchIndexColumn(PGrnCreateData *data,
										 PGrnJSONBCreateData *jsonbData)
{
	const char *tokenizerName = PGRN_DEFAULT_TOKENIZER;
	const char *normalizerName = PGRN_DEFAULT_NORMALIZER;
	char lexiconName[GRN_TABLE_MAX_KEY_SIZE];
	grn_obj *lexicon;

	PGrnApplyOptionValues(data->index, &tokenizerName, &normalizerName);

	if (PGrnIsNoneValue(tokenizerName))
		return;

	snprintf(lexiconName, sizeof(lexiconName),
			 PGrnJSONValueLexiconNameFormat,
			 "FullTextSearch", data->relNode, data->i);
	lexicon = PGrnCreateTable(lexiconName,
							  GRN_OBJ_TABLE_PAT_KEY,
							  grn_ctx_at(ctx, GRN_DB_SHORT_TEXT));
	GRN_PTR_PUT(ctx, data->lexicons, lexicon);

	grn_obj_set_info(ctx, lexicon, GRN_INFO_DEFAULT_TOKENIZER,
					 PGrnLookup(tokenizerName, ERROR));
	if (!PGrnIsNoneValue(normalizerName))
	{
		grn_obj_set_info(ctx, lexicon, GRN_INFO_NORMALIZER,
						 PGrnLookup(normalizerName, ERROR));
	}

	PGrnCreateColumn(lexicon,
					 PGrnIndexColumnName,
					 GRN_OBJ_COLUMN_INDEX | GRN_OBJ_WITH_POSITION,
					 jsonbData->valuesTable);
}

static void
PGrnJSONBCreateIndexColumn(PGrnCreateData *data,
						   PGrnJSONBCreateData *jsonbData,
						   const char *typeName,
						   grn_obj_flags tableType,
						   grn_obj *type)
{
	char lexiconName[GRN_TABLE_MAX_KEY_SIZE];
	grn_obj *lexicon;

	snprintf(lexiconName, sizeof(lexiconName),
			 PGrnJSONValueLexiconNameFormat,
			 typeName, data->relNode, data->i);
	lexicon = PGrnCreateTable(lexiconName, tableType, type);
	GRN_PTR_PUT(ctx, data->lexicons, lexicon);
	PGrnCreateColumn(lexicon,
					 PGrnIndexColumnName,
					 GRN_OBJ_COLUMN_INDEX,
					 jsonbData->valuesTable);
}

static void
PGrnJSONBCreateIndexColumns(PGrnCreateData *data,
							PGrnJSONBCreateData *jsonbData)
{
	PGrnCreateColumn(jsonbData->valuesTable,
					 PGrnIndexColumnName,
					 GRN_OBJ_COLUMN_INDEX,
					 data->sourcesTable);
	PGrnCreateColumn(jsonbData->pathsTable,
					 PGrnIndexColumnName,
					 GRN_OBJ_COLUMN_INDEX | GRN_OBJ_WITH_SECTION,
					 jsonbData->valuesTable);

	/* TODO: 4KiB over string value can't be searched. */
	PGrnJSONBCreateIndexColumn(data,
							   jsonbData,
							   "String",
							   GRN_OBJ_TABLE_PAT_KEY,
							   grn_ctx_at(ctx, GRN_DB_SHORT_TEXT));
	PGrnJSONBCreateIndexColumn(data,
							   jsonbData,
							   "Number",
							   GRN_OBJ_TABLE_PAT_KEY,
							   grn_ctx_at(ctx, GRN_DB_FLOAT));
	PGrnJSONBCreateIndexColumn(data,
							   jsonbData,
							   "Boolean",
							   GRN_OBJ_TABLE_HASH_KEY,
							   grn_ctx_at(ctx, GRN_DB_BOOL));
	PGrnJSONBCreateIndexColumn(data,
							   jsonbData,
							   "Size",
							   GRN_OBJ_TABLE_PAT_KEY,
							   grn_ctx_at(ctx, GRN_DB_UINT32));

	PGrnJSONBCreateFullTextSearchIndexColumn(data, jsonbData);
}
#endif

bool
PGrnAttributeIsJSONB(Oid id)
{
#ifdef PGRN_SUPPORT_JSONB
	return id == JSONBOID;
#else
	return false;
#endif
}

void
PGrnJSONBCreate(PGrnCreateData *data)
{
#ifdef PGRN_SUPPORT_JSONB
	PGrnJSONBCreateData jsonbData;

	if (data->desc->natts != 1)
	{
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("pgroonga: multicolumn index for jsonb "
						"isn't supported: <%s>",
						data->index->rd_rel->relname.data)));
	}

	PGrnJSONBCreateTables(data, &jsonbData);
	PGrnJSONBCreateDataColumns(&jsonbData);
	PGrnJSONBCreateIndexColumns(data, &jsonbData);
	data->attributeTypeID = grn_obj_id(ctx, jsonbData.valuesTable);
	data->attributeFlags = GRN_OBJ_VECTOR;
	PGrnCreateDataColumn(data);
#endif
}

#ifdef PGRN_SUPPORT_JSONB
static void
PGrnJSONBValueSetSource(Relation index,
						grn_obj *jsonValuesTable,
						const char *columnName,
						const char *typeName,
						unsigned int nthAttribute,
						bool required)
{
	char indexName[GRN_TABLE_MAX_KEY_SIZE];
	grn_obj *indexColumn;
	grn_obj *source;

	snprintf(indexName, sizeof(indexName),
			 PGrnJSONValueLexiconNameFormat ".%s",
			 typeName,
			 index->rd_node.relNode,
			 nthAttribute,
			 PGrnIndexColumnName);
	if (required)
	{
		indexColumn = PGrnLookup(indexName, ERROR);
	}
	else
	{
		indexColumn = grn_ctx_get(ctx, indexName, strlen(indexName));
		if (!indexColumn)
			return;
	}

	source = PGrnLookupColumn(jsonValuesTable, columnName, ERROR);
	PGrnIndexColumnSetSource(indexColumn, source);

	grn_obj_unlink(ctx, source);
	grn_obj_unlink(ctx, indexColumn);
}

static void
PGrnJSONBSetSources(Relation index,
					grn_obj *jsonValuesTable,
					unsigned int nthAttribute)
{
	grn_obj *jsonPathsTable;

	jsonPathsTable = PGrnJSONBLookupPathsTable(index, nthAttribute, ERROR);

	{
		grn_obj *source;
		grn_obj *indexColumn;

		GRN_BULK_REWIND(&(buffers->sourceIDs));

		source = PGrnLookupColumn(jsonValuesTable, "path", ERROR);
		GRN_RECORD_PUT(ctx, &(buffers->sourceIDs), grn_obj_id(ctx, source));
		grn_obj_unlink(ctx, source);

		source = PGrnLookupColumn(jsonValuesTable, "paths", ERROR);
		GRN_RECORD_PUT(ctx, &(buffers->sourceIDs), grn_obj_id(ctx, source));
		grn_obj_unlink(ctx, source);

		indexColumn = PGrnLookupColumn(jsonPathsTable, PGrnIndexColumnName,
									   ERROR);
		grn_obj_set_info(ctx, indexColumn, GRN_INFO_SOURCE,
						 &(buffers->sourceIDs));
		grn_obj_unlink(ctx, indexColumn);
	}

	PGrnJSONBValueSetSource(index, jsonValuesTable, "string", "String",
							nthAttribute, true);
	PGrnJSONBValueSetSource(index, jsonValuesTable, "number", "Number",
							nthAttribute, true);
	PGrnJSONBValueSetSource(index, jsonValuesTable, "boolean", "Boolean",
							nthAttribute, true);
	PGrnJSONBValueSetSource(index, jsonValuesTable, "size", "Size",
							nthAttribute, true);
	PGrnJSONBValueSetSource(index, jsonValuesTable, "string", "FullTextSearch",
							nthAttribute, false);

	grn_obj_unlink(ctx, jsonPathsTable);
}
#endif

grn_obj *
PGrnJSONBSetSource(Relation index, unsigned int i)
{
#ifdef PGRN_SUPPORT_JSONB
	grn_obj *jsonValuesTable;
	grn_obj *indexColumn;

	jsonValuesTable = PGrnJSONBLookupValuesTable(index, i, ERROR);
	PGrnJSONBSetSources(index, jsonValuesTable, i);
	indexColumn = PGrnLookupColumn(jsonValuesTable,
								   PGrnIndexColumnName,
								   ERROR);

	return indexColumn;
#else
	return NULL;
#endif
}

#ifdef PGRN_SUPPORT_JSONB
static void
PGrnJSONBDeleteValues(grn_obj *valuesTable, grn_obj *valueIDs)
{
	int i, n;

	n = GRN_BULK_VSIZE(valueIDs) / sizeof(grn_id);
	for (i = 0; i < n; i++)
	{
		grn_id id;
		id = GRN_RECORD_VALUE_AT(valueIDs, i);
		grn_table_delete_by_id(ctx, valuesTable, id);
	}
}

/**
 * pgroonga.match_jsonb(jsonb, query) : bool
 */
Datum
pgroonga_match_jsonb(PG_FUNCTION_ARGS)
{
	Jsonb *jsonb = PG_GETARG_JSONB(0);
	text *query = PG_GETARG_TEXT_PP(1);
	grn_obj valueIDs;
	PGrnJSONBInsertData data;
	JsonbIterator *iter;
	grn_obj *filter = NULL;
	grn_obj *dummy_variable = NULL;
	grn_obj *result = NULL;
	bool matched = false;

	data.pathsTable  = tmpPathsTable;
	data.valuesTable = tmpValuesTable;
	GRN_PTR_INIT(&valueIDs, GRN_OBJ_VECTOR, grn_obj_id(ctx, data.valuesTable));
	data.valueIDs = &valueIDs;
	PGrnJSONBInsertDataInit(&data);
	iter = JsonbIteratorInit(&(jsonb->root));
	PGrnJSONBInsertContainer(&iter, &data);
	PGrnJSONBInsertDataFin(&data);

	PG_TRY();
	{
		GRN_EXPR_CREATE_FOR_QUERY(ctx, tmpValuesTable, filter, dummy_variable);
		PGrnCheck("pgroonga: match_jsonb: failed to create expression object");
		grn_expr_parse(ctx, filter,
					   VARDATA_ANY(query),
					   VARSIZE_ANY_EXHDR(query),
					   NULL, GRN_OP_MATCH, GRN_OP_AND,
					   GRN_EXPR_SYNTAX_SCRIPT);
		PGrnCheck("pgroonga: match_jsonb: failed to parse query");
		result = grn_table_create(ctx, NULL, 0, NULL,
								  GRN_TABLE_HASH_KEY|GRN_OBJ_WITH_SUBREC,
								  tmpValuesTable, NULL);
		PGrnCheck("pgroonga: match_jsonb: failed to create result table");
		grn_table_select(ctx, tmpValuesTable, filter, result, GRN_OP_OR);
		PGrnCheck("pgroonga: match_jsonb: failed to select");
	}
	PG_CATCH();
	{
		if (result)
			grn_obj_close(ctx, result);
		if (filter)
			grn_obj_close(ctx, filter);
		PGrnJSONBDeleteValues(tmpValuesTable, &valueIDs);
		GRN_OBJ_FIN(ctx, &valueIDs);
		PG_RE_THROW();
	}
	PG_END_TRY();

	matched = grn_table_size(ctx, result) > 0;

	grn_obj_close(ctx, filter);
	grn_obj_close(ctx, result);

	PGrnJSONBDeleteValues(tmpValuesTable, &valueIDs);
	GRN_OBJ_FIN(ctx, &valueIDs);

	PG_RETURN_BOOL(matched);
}
#endif

void
PGrnJSONBInsert(Relation index,
				Datum *values,
				unsigned int nthValue,
				grn_obj *valueIDs)
{
#ifdef PGRN_SUPPORT_JSONB
	PGrnJSONBInsertData data;
	Jsonb *jsonb;
	JsonbIterator *iter;

	data.pathsTable  = PGrnJSONBLookupPathsTable(index, nthValue, ERROR);
	data.valuesTable = PGrnJSONBLookupValuesTable(index, nthValue, ERROR);
	data.valueIDs = valueIDs;
	grn_obj_reinit(ctx, data.valueIDs,
				   grn_obj_id(ctx, data.valuesTable),
				   GRN_OBJ_VECTOR);
	PGrnJSONBInsertDataInit(&data);
	jsonb = DatumGetJsonb(values[nthValue]);
	iter = JsonbIteratorInit(&(jsonb->root));
	PGrnJSONBInsertContainer(&iter, &data);
	PGrnJSONBInsertDataFin(&data);
#endif
}

#ifdef PGRN_SUPPORT_JSONB
static void
PGrnSearchBuildConditionJSONQuery(PGrnSearchData *data,
								  grn_obj *subFilter,
								  grn_obj *targetColumn,
								  grn_obj *filter,
								  unsigned int *nthCondition)
{
	grn_expr_append_obj(ctx, data->expression,
						subFilter, GRN_OP_PUSH, 1);
	grn_expr_append_obj(ctx, data->expression,
						targetColumn, GRN_OP_PUSH, 1);
	grn_expr_append_const(ctx, data->expression,
						  filter, GRN_OP_PUSH, 1);
	grn_expr_append_op(ctx, data->expression, GRN_OP_CALL, 2);

	if (*nthCondition > 0)
		grn_expr_append_op(ctx, data->expression, GRN_OP_AND, 2);

	(*nthCondition)++;
}

static void
PGrnSearchBuildConditionJSONContainType(PGrnSearchData *data,
										grn_obj *subFilter,
										grn_obj *targetColumn,
										grn_obj *components,
										const char *typeName,
										unsigned int *nthCondition)
{
	GRN_BULK_REWIND(&(buffers->general));

	GRN_TEXT_PUTS(ctx, &(buffers->general), "type == ");
	grn_text_esc(ctx, &(buffers->general), typeName, strlen(typeName));

	GRN_BULK_REWIND(&(buffers->path));
	PGrnJSONGenerateCompletePath(components, &(buffers->path));
	GRN_TEXT_PUTS(ctx, &(buffers->general), " && path == ");
	grn_text_esc(ctx, &(buffers->general),
				 GRN_TEXT_VALUE(&(buffers->path)),
				 GRN_TEXT_LEN(&(buffers->path)));

	PGrnSearchBuildConditionJSONQuery(data, subFilter, targetColumn,
									  &(buffers->general), nthCondition);
}

static void
PGrnSearchBuildConditionJSONContainValue(PGrnSearchData *data,
										 grn_obj *subFilter,
										 grn_obj *targetColumn,
										 grn_obj *components,
										 JsonbValue *value,
										 unsigned int *nthCondition)
{
	GRN_BULK_REWIND(&(buffers->general));

	switch (value->type)
	{
	case jbvNull:
		GRN_TEXT_PUTS(ctx, &(buffers->general), "type == \"null\"");
		break;
	case jbvString:
		if (value->val.string.len == 0)
			GRN_TEXT_PUTS(ctx, &(buffers->general), "type == \"string\" && ");
		GRN_TEXT_PUTS(ctx, &(buffers->general), "string == ");
		grn_text_esc(ctx, &(buffers->general),
					 value->val.string.val,
					 value->val.string.len);
		break;
	case jbvNumeric:
	{
		Datum numericInString =
			DirectFunctionCall1(numeric_out,
								NumericGetDatum(value->val.numeric));
		const char *numericInCString = DatumGetCString(numericInString);

		if (strcmp(numericInCString, "0") == 0)
			GRN_TEXT_PUTS(ctx, &(buffers->general), "type == \"number\" && ");
		GRN_TEXT_PUTS(ctx, &(buffers->general), "number == ");
		GRN_TEXT_PUTS(ctx, &(buffers->general), numericInCString);
		break;
	}
	case jbvBool:
		GRN_TEXT_PUTS(ctx, &(buffers->general), "type == \"boolean\" && ");
		GRN_TEXT_PUTS(ctx, &(buffers->general), "boolean == ");
		if (value->val.boolean)
			GRN_TEXT_PUTS(ctx, &(buffers->general), "true");
		else
			GRN_TEXT_PUTS(ctx, &(buffers->general), "false");
		break;
	default:
		return;
		break;
	}

	GRN_BULK_REWIND(&(buffers->path));
	PGrnJSONGenerateCompletePath(components, &(buffers->path));
	GRN_TEXT_PUTS(ctx, &(buffers->general), " && path == ");
	grn_text_esc(ctx, &(buffers->general),
				 GRN_TEXT_VALUE(&(buffers->path)),
				 GRN_TEXT_LEN(&(buffers->path)));

	PGrnSearchBuildConditionJSONQuery(data, subFilter, targetColumn,
									  &(buffers->general), nthCondition);
}

static void
PGrnSearchBuildConditionJSONContain(PGrnSearchData *data,
									grn_obj *subFilter,
									grn_obj *targetColumn,
									Jsonb *jsonb)
{
	unsigned int nthCondition = 0;
	grn_obj components;
	JsonbIterator *iter;
	JsonbIteratorToken token;
	JsonbValue value;

	GRN_TEXT_INIT(&components, GRN_OBJ_VECTOR);
	iter = JsonbIteratorInit(&(jsonb->root));
	while ((token = JsonbIteratorNext(&iter, &value, false)) != WJB_DONE) {
		switch (token)
		{
		case WJB_KEY:
			grn_vector_add_element(ctx, &components,
								   value.val.string.val,
								   value.val.string.len,
								   0,
								   GRN_DB_SHORT_TEXT);
			break;
		case WJB_VALUE:
		{
			const char *component;
			PGrnSearchBuildConditionJSONContainValue(data,
													 subFilter,
													 targetColumn,
													 &components,
													 &value,
													 &nthCondition);
			grn_vector_pop_element(ctx, &components, &component, NULL, NULL);
			break;
		}
		case WJB_ELEM:
			PGrnSearchBuildConditionJSONContainValue(data,
													 subFilter,
													 targetColumn,
													 &components,
													 &value,
													 &nthCondition);
			break;
		case WJB_BEGIN_ARRAY:
		{
			uint32_t nElements = value.val.array.nElems;
			grn_vector_add_element(ctx, &components,
								   (const char *)&nElements,
								   sizeof(uint32_t),
								   0,
								   GRN_DB_UINT32);
			if (nElements == 0)
				PGrnSearchBuildConditionJSONContainType(data,
														subFilter,
														targetColumn,
														&components,
														"array",
														&nthCondition);
			break;
		}
		case WJB_END_ARRAY:
		{
			const char *component;
			grn_vector_pop_element(ctx, &components, &component,
								   NULL, NULL);
			break;
		}
		case WJB_BEGIN_OBJECT:
			if (value.val.object.nPairs == 0)
				PGrnSearchBuildConditionJSONContainType(data,
														subFilter,
														targetColumn,
														&components,
														"object",
														&nthCondition);
			break;
		case WJB_END_OBJECT:
			break;
		default:
			ereport(ERROR,
					(errcode(ERRCODE_SYSTEM_ERROR),
					 errmsg("pgroonga: jsonb iterator returns invalid token: %d",
							token)));
			break;
		}
	}
	GRN_OBJ_FIN(ctx, &components);
}
#endif

bool
PGrnJSONBBuildSearchCondition(PGrnSearchData *data,
							  ScanKey key,
							  grn_obj *targetColumn)
{
#ifdef PGRN_SUPPORT_JSONB
	grn_obj *subFilter;

	subFilter = PGrnLookup("sub_filter", ERROR);
	grn_obj_reinit(ctx, &(buffers->general), GRN_DB_TEXT, 0);

	if (key->sk_strategy == PGrnQueryStrategyNumber)
	{
		unsigned int nthCondition = 0;
		PGrnConvertFromData(key->sk_argument, TEXTOID, &(buffers->general));
		PGrnSearchBuildConditionJSONQuery(data,
										  subFilter,
										  targetColumn,
										  &(buffers->general),
										  &nthCondition);
	}
	else
	{
		PGrnSearchBuildConditionJSONContain(data,
											subFilter,
											targetColumn,
											DatumGetJsonb(key->sk_argument));
	}
	return true;
#else
	return false;
#endif
}

#ifdef PGRN_SUPPORT_JSONB
static void
PGrnJSONValuesUpdateDeletedID(grn_obj *jsonValuesTable,
							  grn_obj *values,
							  grn_obj *valueMin,
							  grn_obj *valueMax)
{
	unsigned int i, n;

	n = GRN_BULK_VSIZE(values) / sizeof(grn_id);
	for (i = 0; i < n; i++)
	{
		grn_id valueID = GRN_RECORD_VALUE_AT(values, i);

		if (GRN_RECORD_VALUE(valueMin) == GRN_ID_NIL ||
			GRN_RECORD_VALUE(valueMin) > valueID)
		{
			GRN_RECORD_SET(ctx, valueMin, valueID);
		}

		if (GRN_RECORD_VALUE(valueMax) == GRN_ID_NIL ||
			GRN_RECORD_VALUE(valueMax) < valueID)
		{
			GRN_RECORD_SET(ctx, valueMax, valueID);
		}
	}
}

static void
PGrnJSONValuesDeleteBulk(grn_obj *jsonValuesTable,
						 grn_obj *jsonValuesIndexColumn,
						 grn_obj *valueMin,
						 grn_obj *valueMax)
{
	char minKey[GRN_TABLE_MAX_KEY_SIZE];
	char maxKey[GRN_TABLE_MAX_KEY_SIZE];
	unsigned int minKeySize;
	unsigned int maxKeySize;
	grn_table_cursor *tableCursor;
	grn_id valueID;

	minKeySize = grn_table_get_key(ctx,
								   jsonValuesTable,
								   GRN_RECORD_VALUE(valueMin),
								   minKey,
								   sizeof(minKey));
	maxKeySize = grn_table_get_key(ctx,
								   jsonValuesTable,
								   GRN_RECORD_VALUE(valueMax),
								   maxKey,
								   sizeof(maxKey));
	tableCursor = grn_table_cursor_open(ctx, jsonValuesTable,
										minKey, minKeySize,
										maxKey, maxKeySize,
										0, -1, 0);
	if (!tableCursor)
		return;

	while ((valueID = grn_table_cursor_next(ctx, tableCursor)) != GRN_ID_NIL)
	{
		grn_ii_cursor *iiCursor;
		bool haveReference = false;

		iiCursor = grn_ii_cursor_open(ctx,
									  (grn_ii *)jsonValuesIndexColumn,
									  valueID,
									  GRN_ID_NIL, GRN_ID_MAX, 0, 0);
		if (iiCursor)
		{
			while (grn_ii_cursor_next(ctx, iiCursor))
			{
				haveReference = true;
				break;
			}
			grn_ii_cursor_close(ctx, iiCursor);
		}

		if (!haveReference)
			grn_table_cursor_delete(ctx, tableCursor);
	}

	grn_table_cursor_close(ctx, tableCursor);
}
#endif

void
PGrnJSONBBulkDeleteInit(PGrnJSONBBulkDeleteData *data)
{
#ifdef PGRN_SUPPORT_JSONB
	Relation index = data->index;
	TupleDesc desc;
	Form_pg_attribute attribute;
	grn_id valuesTableID;

	desc = RelationGetDescr(index);
	attribute = desc->attrs[0];
	data->isJSONBAttribute = PGrnAttributeIsJSONB(attribute->atttypid);
	if (!data->isJSONBAttribute)
		return;

	data->sourcesValuesColumn = PGrnLookupColumn(data->sourcesTable,
												 attribute->attname.data,
												 ERROR);
	data->valuesTable = PGrnJSONBLookupValuesTable(index, 0, ERROR);
	data->valuesIndexColumn = PGrnLookupColumn(data->valuesTable,
											   PGrnIndexColumnName,
											   ERROR);

	valuesTableID = grn_obj_id(ctx, data->valuesTable);
	GRN_RECORD_INIT(&(data->values),   0, valuesTableID);
	GRN_RECORD_INIT(&(data->valueMin), 0, valuesTableID);
	GRN_RECORD_INIT(&(data->valueMax), 0, valuesTableID);

	GRN_RECORD_SET(ctx, &(data->valueMin), GRN_ID_NIL);
	GRN_RECORD_SET(ctx, &(data->valueMax), GRN_ID_NIL);
#endif
}

void
PGrnJSONBBulkDeleteRecord(PGrnJSONBBulkDeleteData *data)
{
#ifdef PGRN_SUPPORT_JSONB
	if (!data->isJSONBAttribute)
		return;

	GRN_BULK_REWIND(&(data->values));
	grn_obj_get_value(ctx,
					  data->sourcesValuesColumn,
					  data->id,
					  &(data->values));
	PGrnJSONValuesUpdateDeletedID(data->valuesTable,
								  &(data->values),
								  &(data->valueMin),
								  &(data->valueMax));
#endif
}

void
PGrnJSONBBulkDeleteFin(PGrnJSONBBulkDeleteData *data)
{
#ifdef PGRN_SUPPORT_JSONB
	if (!data->isJSONBAttribute)
		return;

	PGrnJSONValuesDeleteBulk(data->valuesTable,
							 data->valuesIndexColumn,
							 &(data->valueMin),
							 &(data->valueMax));

	GRN_OBJ_FIN(ctx, &(data->values));
	grn_obj_unlink(ctx, data->sourcesValuesColumn);
	grn_obj_unlink(ctx, data->valuesIndexColumn);
	grn_obj_unlink(ctx, data->valuesTable);
#endif
}

#ifdef PGRN_SUPPORT_JSONB
static bool
PGrnRemoveJSONValueLexicon(const char *typeName, unsigned int relationID)
{
	char tableName[GRN_TABLE_MAX_KEY_SIZE];
	snprintf(tableName, sizeof(tableName),
			 PGrnJSONValueLexiconNameFormat,
			 typeName, relationID, 0);
	return PGrnRemoveObject(tableName);
}
#endif

void
PGrnJSONBRemoveUnusedTables(Oid relationFileNodeID)
{
#ifdef PGRN_SUPPORT_JSONB
		PGrnRemoveJSONValueLexicon("FullTextSearch", relationFileNodeID);
		PGrnRemoveJSONValueLexicon("String", relationFileNodeID);
		PGrnRemoveJSONValueLexicon("Number", relationFileNodeID);
		PGrnRemoveJSONValueLexicon("Boolean", relationFileNodeID);
		PGrnRemoveJSONValueLexicon("Size", relationFileNodeID);

		{
			char name[GRN_TABLE_MAX_KEY_SIZE];

			snprintf(name, sizeof(name),
					 PGrnJSONPathsTableNameFormat ".%s",
					 relationFileNodeID, 0, PGrnIndexColumnName);
			PGrnRemoveObject(name);

			snprintf(name, sizeof(name),
					 PGrnJSONValuesTableNameFormat,
					 relationFileNodeID, 0);
			PGrnRemoveObject(name);

			snprintf(name, sizeof(name),
					 PGrnJSONPathsTableNameFormat,
					 relationFileNodeID, 0);
			PGrnRemoveObject(name);

			snprintf(name, sizeof(name),
					 PGrnJSONTypesTableNameFormat,
					 relationFileNodeID, 0);
			PGrnRemoveObject(name);
		}
#endif
}
