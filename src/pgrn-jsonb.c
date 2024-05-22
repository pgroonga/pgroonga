#include "pgroonga.h"

#include "pgrn-convert.h"
#include "pgrn-global.h"
#include "pgrn-groonga.h"
#include "pgrn-jsonb.h"
#include "pgrn-options.h"
#include "pgrn-value.h"
#include "pgrn-wal.h"

#include <catalog/pg_type.h>
#include <utils/builtins.h>
#include <utils/lsyscache.h>

#include <groonga.h>

#include <xxhash.h>

#include <string.h>

PGDLLEXPORT PG_FUNCTION_INFO_V1(pgroonga_match_script_jsonb);

/* v2 */
PGDLLEXPORT PG_FUNCTION_INFO_V1(pgroonga_match_jsonb);
PGDLLEXPORT PG_FUNCTION_INFO_V1(pgroonga_query_jsonb);
PGDLLEXPORT PG_FUNCTION_INFO_V1(pgroonga_script_jsonb);

typedef struct
{
	grn_obj *pathsTable;
	grn_obj *typesTable;
	grn_obj *valuesTable;
} PGrnJSONBCreateData;

typedef struct PGrnJSONBInsertData
{
	Relation index;
	bool isForFullTextSearchOnly;
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
	grn_obj *tokenStack;
	grn_obj key;
	grn_obj components;
	grn_obj valueKey;
	grn_obj path;
	grn_obj pathIDs;
	grn_obj paths;
	grn_obj value;
	grn_obj type;
} PGrnJSONBInsertData;

static struct PGrnBuffers *buffers = &PGrnBuffers;

static const unsigned int PGRN_JSON_GENERATE_PATH_IS_ABSOLUTE = 1 << 0;
static const unsigned int PGRN_JSON_GENERATE_PATH_INCLUDE_ARRAY = 1 << 1;
static const unsigned int PGRN_JSON_GENERATE_PATH_USE_DOT_STYLE = 1 << 2;

static grn_obj *tmpPathsTable = NULL;
static grn_obj *tmpTypesTable = NULL;
static grn_obj *tmpValuesTable = NULL;

const char *
PGrnJSONBIteratorTokenToString(JsonbIteratorToken token)
{
	switch (token)
	{
	case WJB_DONE:
		return "done";
	case WJB_KEY:
		return "key";
	case WJB_VALUE:
		return "value";
	case WJB_ELEM:
		return "element";
	case WJB_BEGIN_ARRAY:
		return "begin-array";
	case WJB_END_ARRAY:
		return "end-array";
	case WJB_BEGIN_OBJECT:
		return "begin-object";
	case WJB_END_OBJECT:
		return "end-object";
	default:
		return "unknown";
	}
}

const char *
PGrnJSONBValueTypeToString(enum jbvType type)
{
	switch (type)
	{
	case jbvNull:
		return "null";
	case jbvString:
		return "string";
	case jbvNumeric:
		return "numeric";
	case jbvBool:
		return "bool";
	case jbvArray:
		return "array";
	case jbvObject:
		return "object";
	case jbvBinary:
		return "binary";
#ifdef PGRN_HAVE_JSONB_DATETIME
	case jbvDatetime:
		return "datetime";
#endif
	default:
		return "unknown";
	}
}

Jsonb *
PGrnJSONBParse(const char *jsonString)
{
	Datum jsonbDatum =
		DirectFunctionCall1(jsonb_in, CStringGetDatum(jsonString));
	return DatumGetJsonbP(jsonbDatum);
}

grn_obj *
PGrnJSONBLookupValuesTable(Relation index,
						   unsigned int nthAttribute,
						   int errorLevel)
{
	char name[GRN_TABLE_MAX_KEY_SIZE];

	snprintf(name,
			 sizeof(name),
			 PGrnJSONValuesTableNameFormat,
			 PGRN_RELATION_GET_LOCATOR_NUMBER(index),
			 nthAttribute);
	return PGrnLookup(name, errorLevel);
}

grn_obj *
PGrnJSONBLookupPathsTable(Relation index,
						  unsigned int nthAttribute,
						  int errorLevel)
{
	char name[GRN_TABLE_MAX_KEY_SIZE];

	snprintf(name,
			 sizeof(name),
			 PGrnJSONPathsTableNameFormat,
			 PGRN_RELATION_GET_LOCATOR_NUMBER(index),
			 nthAttribute);
	return PGrnLookup(name, errorLevel);
}

grn_obj *
PGrnJSONBLookupTypesTable(Relation index,
						  unsigned int nthAttribute,
						  int errorLevel)
{
	char name[GRN_TABLE_MAX_KEY_SIZE];

	snprintf(name,
			 sizeof(name),
			 PGrnJSONTypesTableNameFormat,
			 PGRN_RELATION_GET_LOCATOR_NUMBER(index),
			 nthAttribute);
	return PGrnLookup(name, errorLevel);
}

grn_obj *
PGrnJSONBLookupFullTextSearchLexicon(Relation index,
									 unsigned int nthAttribute,
									 int errorLevel)
{
	char name[GRN_TABLE_MAX_KEY_SIZE];

	snprintf(name,
			 sizeof(name),
			 PGrnJSONValueLexiconNameFormat,
			 "FullTextSearch",
			 PGRN_RELATION_GET_LOCATOR_NUMBER(index),
			 nthAttribute);
	return PGrnLookup(name, errorLevel);
}

grn_obj *
PGrnJSONBLookupStringLexicon(Relation index,
							 unsigned int nthAttribute,
							 int errorLevel)
{
	char name[GRN_TABLE_MAX_KEY_SIZE];

	snprintf(name,
			 sizeof(name),
			 PGrnJSONValueLexiconNameFormat,
			 "String",
			 PGRN_RELATION_GET_LOCATOR_NUMBER(index),
			 nthAttribute);
	return PGrnLookup(name, errorLevel);
}

grn_obj *
PGrnJSONBLookupNumberLexicon(Relation index,
							 unsigned int nthAttribute,
							 int errorLevel)
{
	char name[GRN_TABLE_MAX_KEY_SIZE];

	snprintf(name,
			 sizeof(name),
			 PGrnJSONValueLexiconNameFormat,
			 "Number",
			 PGRN_RELATION_GET_LOCATOR_NUMBER(index),
			 nthAttribute);
	return PGrnLookup(name, errorLevel);
}

grn_obj *
PGrnJSONBLookupBooleanLexicon(Relation index,
							  unsigned int nthAttribute,
							  int errorLevel)
{
	char name[GRN_TABLE_MAX_KEY_SIZE];

	snprintf(name,
			 sizeof(name),
			 PGrnJSONValueLexiconNameFormat,
			 "Boolean",
			 PGRN_RELATION_GET_LOCATOR_NUMBER(index),
			 nthAttribute);
	return PGrnLookup(name, errorLevel);
}

grn_obj *
PGrnJSONBLookupSizeLexicon(Relation index,
						   unsigned int nthAttribute,
						   int errorLevel)
{
	char name[GRN_TABLE_MAX_KEY_SIZE];

	snprintf(name,
			 sizeof(name),
			 PGrnJSONValueLexiconNameFormat,
			 "Size",
			 PGRN_RELATION_GET_LOCATOR_NUMBER(index),
			 nthAttribute);
	return PGrnLookup(name, errorLevel);
}

static grn_obj *
PGrnJSONBCreatePathsTable(Relation index, const char *name)
{
	return PGrnCreateTable(index,
						   name,
						   GRN_OBJ_TABLE_PAT_KEY,
						   grn_ctx_at(ctx, GRN_DB_SHORT_TEXT),
						   NULL,
						   NULL,
						   NULL);
}

static grn_obj *
PGrnJSONBCreateTypesTable(Relation index, const char *name)
{
	return PGrnCreateTable(index,
						   name,
						   GRN_OBJ_TABLE_PAT_KEY,
						   grn_ctx_at(ctx, GRN_DB_SHORT_TEXT),
						   NULL,
						   NULL,
						   NULL);
}

static grn_obj *
PGrnJSONBCreateValuesTable(Relation index, const char *name)
{
	return PGrnCreateTable(index,
						   name,
						   GRN_OBJ_TABLE_HASH_KEY,
						   grn_ctx_at(ctx, GRN_DB_UINT64),
						   NULL,
						   NULL,
						   NULL);
}

static void
PGrnJSONBCreateDataColumns(Relation index, PGrnJSONBCreateData *jsonbData)
{
	PGrnCreateColumn(index,
					 jsonbData->valuesTable,
					 "path",
					 GRN_OBJ_COLUMN_SCALAR,
					 jsonbData->pathsTable);
	PGrnCreateColumn(index,
					 jsonbData->valuesTable,
					 "paths",
					 GRN_OBJ_COLUMN_VECTOR,
					 jsonbData->pathsTable);
	{
		grn_column_flags flags = 0;
		if (PGrnIsZstdAvailable)
		{
			flags |= GRN_OBJ_COMPRESS_ZSTD;
		}
		else if (PGrnIsLZ4Available)
		{
			flags |= GRN_OBJ_COMPRESS_LZ4;
		}
		else if (PGrnIsZlibAvailable)
		{
			flags |= GRN_OBJ_COMPRESS_ZLIB;
		}
		PGrnCreateColumn(index,
						 jsonbData->valuesTable,
						 "string",
						 flags,
						 grn_ctx_at(ctx, GRN_DB_LONG_TEXT));
	}
	PGrnCreateColumn(index,
					 jsonbData->valuesTable,
					 "number",
					 0,
					 grn_ctx_at(ctx, GRN_DB_FLOAT));
	PGrnCreateColumn(index,
					 jsonbData->valuesTable,
					 "boolean",
					 0,
					 grn_ctx_at(ctx, GRN_DB_BOOL));
	PGrnCreateColumn(index,
					 jsonbData->valuesTable,
					 "size",
					 0,
					 grn_ctx_at(ctx, GRN_DB_UINT32));
	PGrnCreateColumn(
		index, jsonbData->valuesTable, "type", 0, jsonbData->typesTable);
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

		componentSize = grn_vector_get_element(
			ctx, components, i, &component, NULL, &domain);
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

static void
PGrnJSONBInsertDataInit(PGrnJSONBInsertData *data)
{
	if (data->isForFullTextSearchOnly)
	{
		GRN_TEXT_INIT(&(data->value), GRN_OBJ_VECTOR);
		return;
	}

	data->pathColumn = PGrnLookupColumn(data->valuesTable, "path", ERROR);
	data->pathsColumn = PGrnLookupColumn(data->valuesTable, "paths", ERROR);
	data->stringColumn = PGrnLookupColumn(data->valuesTable, "string", ERROR);
	data->numberColumn = PGrnLookupColumn(data->valuesTable, "number", ERROR);
	data->booleanColumn = PGrnLookupColumn(data->valuesTable, "boolean", ERROR);
	data->sizeColumn = PGrnLookupColumn(data->valuesTable, "size", ERROR);
	data->typeColumn = PGrnLookupColumn(data->valuesTable, "type", ERROR);

	GRN_TEXT_INIT(&(data->key), 0);
	GRN_TEXT_INIT(&(data->components), GRN_OBJ_VECTOR);
	GRN_UINT64_INIT(&(data->valueKey), 0);
	GRN_TEXT_INIT(&(data->path), 0);
	GRN_RECORD_INIT(
		&(data->pathIDs), GRN_OBJ_VECTOR, grn_obj_id(ctx, data->pathsTable));
	GRN_TEXT_INIT(&(data->paths), GRN_OBJ_VECTOR);
	GRN_VOID_INIT(&(data->value));
	GRN_TEXT_INIT(&(data->type), GRN_OBJ_DO_SHALLOW_COPY);
}

static void
PGrnJSONBInsertDataFin(PGrnJSONBInsertData *data)
{
	if (data->isForFullTextSearchOnly)
	{
		GRN_OBJ_FIN(ctx, &(data->value));
		return;
	}

	GRN_OBJ_FIN(ctx, &(data->type));
	GRN_OBJ_FIN(ctx, &(data->value));
	GRN_OBJ_FIN(ctx, &(data->paths));
	GRN_OBJ_FIN(ctx, &(data->pathIDs));
	GRN_OBJ_FIN(ctx, &(data->path));
	GRN_OBJ_FIN(ctx, &(data->valueKey));
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

		componentSize = grn_vector_get_element(
			ctx, &(data->components), i, &component, NULL, &domain);
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

	return XXH64(GRN_TEXT_VALUE(&data->key), GRN_TEXT_LEN(&data->key), 0);
}

static void
PGrnJSONBInsertAddPath(PGrnJSONBInsertData *data,
					   unsigned int start,
					   unsigned int flags)
{
	grn_id pathID;

	GRN_BULK_REWIND(&(data->path));
	PGrnJSONGeneratePath(&(data->components), start, flags, &(data->path));

	if (GRN_TEXT_LEN(&(data->path)) == 0)
		return;

	if (GRN_TEXT_LEN(&(data->path)) >= GRN_TABLE_MAX_KEY_SIZE)
		return;

	pathID = grn_table_add(ctx,
						   data->pathsTable,
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

	PGrnJSONBInsertAddPath(
		data, parentStart, PGRN_JSON_GENERATE_PATH_USE_DOT_STYLE);
	PGrnJSONBInsertAddPath(data, parentStart, 0);
	PGrnJSONBInsertAddPath(
		data, parentStart, PGRN_JSON_GENERATE_PATH_INCLUDE_ARRAY);

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
	PGrnJSONBInsertAddPath(data, 0, PGRN_JSON_GENERATE_PATH_IS_ABSOLUTE);
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
	size_t nColumns = 3; /* _key, path?, paths, column?, type */
	bool setPath;
	PGrnWALData *walData = NULL;

	key = PGrnJSONBInsertGenerateKey(data, column != NULL, typeName);
	valueID =
		grn_table_add(ctx, data->valuesTable, &key, sizeof(uint64_t), &added);
	GRN_RECORD_PUT(ctx, data->valueIDs, valueID);
	if (!added)
		return;

	GRN_BULK_REWIND(&(data->path));
	PGrnJSONGenerateCompletePath(&(data->components), &(data->path));
	setPath = (GRN_TEXT_LEN(&(data->path)) < GRN_TABLE_MAX_KEY_SIZE);
	if (setPath)
		nColumns++;

	if (column)
		nColumns++;

	walData = PGrnWALStart(data->index);
	PGrnWALInsertStart(walData, data->valuesTable, nColumns);
	PGrnWALInsertKeyRaw(walData, &key, sizeof(uint64_t));

	if (setPath)
	{
		grn_obj_set_value(
			ctx, data->pathColumn, valueID, &(data->path), GRN_OBJ_SET);
		PGrnWALInsertColumn(walData, data->pathColumn, &(data->path));
	}

	PGrnJSONBInsertGeneratePaths(data);
	grn_obj_set_value(
		ctx, data->pathsColumn, valueID, &(data->pathIDs), GRN_OBJ_SET);
	if (walData)
	{
		unsigned int i, n;

		GRN_BULK_REWIND(&(data->paths));
		n = GRN_BULK_VSIZE(&(data->pathIDs)) / sizeof(grn_id);
		for (i = 0; i < n; i++)
		{
			grn_id pathID;
			char path[GRN_TABLE_MAX_KEY_SIZE];
			int pathSize;

			pathID = GRN_RECORD_VALUE_AT(&(data->pathIDs), i);
			pathSize = grn_table_get_key(
				ctx, data->pathsTable, pathID, path, GRN_TABLE_MAX_KEY_SIZE);
			grn_vector_add_element(
				ctx, &(data->paths), path, pathSize, 0, GRN_DB_SHORT_TEXT);
		}
		PGrnWALInsertColumn(walData, data->pathsColumn, &(data->paths));
	}

	if (column)
	{
		grn_obj_set_value(ctx, column, valueID, &(data->value), GRN_OBJ_SET);
		PGrnWALInsertColumn(walData, column, &(data->value));
	}

	GRN_TEXT_SETS(ctx, &(data->type), typeName);
	grn_obj_set_value(
		ctx, data->typeColumn, valueID, &(data->type), GRN_OBJ_SET);
	PGrnWALInsertColumn(walData, data->typeColumn, &(data->type));

	PGrnWALInsertFinish(walData);
	PGrnWALFinish(walData);
}

static void PGrnJSONBInsertContainer(JsonbIterator **iter,
									 PGrnJSONBInsertData *data);

static void
PGrnJSONBInsertValue(JsonbIterator **iter,
					 JsonbValue *value,
					 PGrnJSONBInsertData *data)
{
#ifdef PGRN_HAVE_JSONB_DATETIME
	const char *tag = "[jsonb][insert]";
#endif

	switch (value->type)
	{
	case jbvNull:
		PGrnJSONBInsertValueSet(data, NULL, "null");
		break;
	case jbvString:
		grn_obj_reinit(
			ctx, &(data->value), GRN_DB_LONG_TEXT, GRN_OBJ_DO_SHALLOW_COPY);
		GRN_TEXT_SET(
			ctx, &(data->value), value->val.string.val, value->val.string.len);
		PGrnJSONBInsertValueSet(data, data->stringColumn, "string");
		break;
	case jbvNumeric:
	{
		Datum numericInString;
		const char *numericInCString;

		numericInString = DirectFunctionCall1(
			numeric_out, NumericGetDatum(value->val.numeric));
		numericInCString = DatumGetCString(numericInString);
		grn_obj_reinit(
			ctx, &(data->value), GRN_DB_TEXT, GRN_OBJ_DO_SHALLOW_COPY);
		GRN_TEXT_SETS(ctx, &(data->value), numericInCString);
		PGrnJSONBInsertValueSet(data, data->numberColumn, "number");
		break;
	}
	case jbvBool:
		grn_obj_reinit(ctx, &(data->value), GRN_DB_BOOL, 0);
		GRN_BOOL_SET(ctx, &(data->value), value->val.boolean);
		PGrnJSONBInsertValueSet(data, data->booleanColumn, "boolean");
		break;
#ifdef PGRN_HAVE_JSONB_DATETIME
	case jbvDatetime:
		PGrnCheckRC(GRN_FUNCTION_NOT_IMPLEMENTED,
					"%s datetime value isn't supported",
					tag);
		break;
#endif
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
	const char *tag = "[jsonb][insert][container]";
	JsonbIteratorToken token;
	JsonbIteratorToken lastToken = WJB_DONE;
	JsonbValue value;

	/* Suppress -Wunused-but-set-variable */
	(void) lastToken;

	GRN_BULK_REWIND(data->tokenStack);
	while ((token = JsonbIteratorNext(iter, &value, false)) != WJB_DONE)
	{
		switch (token)
		{
		case WJB_KEY:
			GRN_UINT8_PUT(ctx, data->tokenStack, token);
			grn_vector_add_element(ctx,
								   &(data->components),
								   value.val.string.val,
								   value.val.string.len,
								   0,
								   GRN_DB_SHORT_TEXT);
			break;
		case WJB_VALUE:
			/* WJB_VALUE isn't emitted when a value is an array or an
			   object. */
			PGrnJSONBInsertValue(iter, &value, data);
			{
				const char *component;
				grn_vector_pop_element(
					ctx, &(data->components), &component, NULL, NULL);
			}
			GRN_UINT8_POP(data->tokenStack, lastToken);
			break;
		case WJB_ELEM:
			PGrnJSONBInsertValue(iter, &value, data);
			break;
		case WJB_BEGIN_ARRAY:
		{
			uint32_t nElements;
			GRN_UINT8_PUT(ctx, data->tokenStack, token);
			nElements = value.val.array.nElems;
			grn_vector_add_element(ctx,
								   &(data->components),
								   (const char *) &nElements,
								   sizeof(uint32_t),
								   0,
								   GRN_DB_UINT32);
			PGrnJSONBInsertValueSet(data, NULL, "array");
			break;
		}
		case WJB_END_ARRAY:
		{
			const char *component;
			size_t nTokens;
			grn_vector_pop_element(
				ctx, &(data->components), &component, NULL, NULL);
			GRN_UINT8_POP(data->tokenStack, lastToken);
			nTokens = GRN_UINT8_VECTOR_SIZE(data->tokenStack);
			if (nTokens > 0 &&
				GRN_UINT8_VALUE_AT(data->tokenStack, nTokens - 1) == WJB_KEY)
			{
				grn_vector_pop_element(
					ctx, &(data->components), &component, NULL, NULL);
				GRN_UINT8_POP(data->tokenStack, lastToken);
			}
			break;
		}
		case WJB_BEGIN_OBJECT:
			GRN_UINT8_PUT(ctx, data->tokenStack, token);
			PGrnJSONBInsertValueSet(data, NULL, "object");
			break;
		case WJB_END_OBJECT:
		{
			size_t nTokens;
			GRN_UINT8_POP(data->tokenStack, lastToken);
			nTokens = GRN_UINT8_VECTOR_SIZE(data->tokenStack);
			if (nTokens > 0 &&
				GRN_UINT8_VALUE_AT(data->tokenStack, nTokens - 1) == WJB_KEY)
			{
				const char *component;
				grn_vector_pop_element(
					ctx, &(data->components), &component, NULL, NULL);
				GRN_UINT8_POP(data->tokenStack, lastToken);
			}
			break;
		}
		default:
			PGrnCheckRC(GRN_UNKNOWN_ERROR,
						"%s iterator returns invalid token: %s",
						tag,
						PGrnJSONBIteratorTokenToString(token));
			break;
		}
	}
}

static void
PGrnJSONBInsertContainerForFullTextSearch(JsonbIterator **iter,
										  PGrnJSONBInsertData *data);

static void
PGrnJSONBInsertValueForFullTextSearch(JsonbIterator **iter,
									  JsonbValue *value,
									  PGrnJSONBInsertData *data)
{
	switch (value->type)
	{
	case jbvNull:
		break;
	case jbvString:
		grn_vector_add_element(ctx,
							   &(data->value),
							   value->val.string.val,
							   value->val.string.len,
							   0,
							   GRN_DB_TEXT);
		break;
	case jbvNumeric:
	case jbvBool:
#ifdef PGRN_HAVE_JSONB_DATETIME
	case jbvDatetime:
#endif
		break;
	case jbvArray:
		PGrnJSONBInsertContainerForFullTextSearch(iter, data);
		break;
	case jbvObject:
		PGrnJSONBInsertContainerForFullTextSearch(iter, data);
		break;
	case jbvBinary:
		PGrnJSONBInsertContainerForFullTextSearch(iter, data);
		break;
	}
}

static void
PGrnJSONBInsertContainerForFullTextSearch(JsonbIterator **iter,
										  PGrnJSONBInsertData *data)
{
	const char *tag = "[jsonb][insert][container][full-text-search]";
	JsonbIteratorToken token;
	JsonbValue value;

	while ((token = JsonbIteratorNext(iter, &value, false)) != WJB_DONE)
	{
		switch (token)
		{
		case WJB_KEY:
			break;
		case WJB_VALUE:
		case WJB_ELEM:
			PGrnJSONBInsertValueForFullTextSearch(iter, &value, data);
			break;
		case WJB_BEGIN_ARRAY:
		case WJB_END_ARRAY:
		case WJB_BEGIN_OBJECT:
		case WJB_END_OBJECT:
			break;
		default:
			PGrnCheckRC(GRN_UNKNOWN_ERROR,
						"%s iterator returns invalid token: %s",
						tag,
						PGrnJSONBIteratorTokenToString(token));
			break;
		}
	}
}

void
PGrnInitializeJSONB(void)
{
	PGrnJSONBCreateData data;

	tmpPathsTable = PGrnJSONBCreatePathsTable(InvalidRelation, NULL);
	tmpTypesTable = PGrnJSONBCreateTypesTable(InvalidRelation, NULL);
	tmpValuesTable = PGrnJSONBCreateValuesTable(InvalidRelation, NULL);

	data.pathsTable = tmpPathsTable;
	data.typesTable = tmpTypesTable;
	data.valuesTable = tmpValuesTable;
	PGrnJSONBCreateDataColumns(InvalidRelation, &data);
}

void
PGrnFinalizeJSONB(void)
{
	grn_obj_close(ctx, tmpValuesTable);
	grn_obj_close(ctx, tmpTypesTable);
	grn_obj_close(ctx, tmpPathsTable);
}

static void
PGrnJSONBCreateTables(PGrnCreateData *data, PGrnJSONBCreateData *jsonbData)
{
	{
		char jsonPathsTableName[GRN_TABLE_MAX_KEY_SIZE];
		snprintf(jsonPathsTableName,
				 sizeof(jsonPathsTableName),
				 PGrnJSONPathsTableNameFormat,
				 data->relNumber,
				 data->i);
		jsonbData->pathsTable =
			PGrnJSONBCreatePathsTable(data->index, jsonPathsTableName);
		GRN_PTR_PUT(ctx, data->supplementaryTables, jsonbData->pathsTable);
	}

	{
		char jsonTypesTableName[GRN_TABLE_MAX_KEY_SIZE];
		snprintf(jsonTypesTableName,
				 sizeof(jsonTypesTableName),
				 PGrnJSONTypesTableNameFormat,
				 data->relNumber,
				 data->i);
		jsonbData->typesTable =
			PGrnJSONBCreateTypesTable(data->index, jsonTypesTableName);
		GRN_PTR_PUT(ctx, data->supplementaryTables, jsonbData->typesTable);
	}

	{
		char jsonValuesTableName[GRN_TABLE_MAX_KEY_SIZE];
		snprintf(jsonValuesTableName,
				 sizeof(jsonValuesTableName),
				 PGrnJSONValuesTableNameFormat,
				 data->relNumber,
				 data->i);
		jsonbData->valuesTable =
			PGrnJSONBCreateValuesTable(data->index, jsonValuesTableName);
		GRN_PTR_PUT(ctx, data->supplementaryTables, jsonbData->valuesTable);
	}
}

static void
PGrnJSONBCreateFullTextSearchIndexColumn(PGrnCreateData *data,
										 PGrnJSONBCreateData *jsonbData)
{
	char lexiconName[GRN_TABLE_MAX_KEY_SIZE];
	grn_table_flags flags = 0;
	grn_obj *type;
	grn_obj *lexicon;
	grn_obj *tokenizer = NULL;
	grn_obj *normalizers = NULL;
	grn_obj *tokenFilters = NULL;
	grn_obj *plugins = NULL;
	grn_column_flags indexFlags = GRN_OBJ_COLUMN_INDEX | GRN_OBJ_WITH_POSITION;

	PGrnApplyOptionValues(data->index,
						  data->i,
						  PGRN_OPTION_USE_CASE_FULL_TEXT_SEARCH,
						  &tokenizer,
						  PGRN_DEFAULT_TOKENIZER,
						  &normalizers,
						  PGRN_DEFAULT_NORMALIZERS,
						  &tokenFilters,
						  &plugins,
						  &flags,
						  &indexFlags);

	if (!tokenizer)
		return;

	PGrnWALRegisterPlugins(data->index, plugins);

	snprintf(lexiconName,
			 sizeof(lexiconName),
			 PGrnJSONValueLexiconNameFormat,
			 "FullTextSearch",
			 data->relNumber,
			 data->i);
	type = grn_ctx_at(ctx, GRN_DB_SHORT_TEXT);
	lexicon = PGrnCreateTable(data->index,
							  lexiconName,
							  flags,
							  type,
							  tokenizer,
							  normalizers,
							  tokenFilters);
	GRN_PTR_PUT(ctx, data->lexicons, lexicon);

	PGrnCreateColumn(data->index,
					 lexicon,
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

	snprintf(lexiconName,
			 sizeof(lexiconName),
			 PGrnJSONValueLexiconNameFormat,
			 typeName,
			 data->relNumber,
			 data->i);
	lexicon = PGrnCreateTable(
		data->index, lexiconName, tableType, type, NULL, NULL, NULL);
	GRN_PTR_PUT(ctx, data->lexicons, lexicon);
	PGrnCreateColumn(data->index,
					 lexicon,
					 PGrnIndexColumnName,
					 GRN_OBJ_COLUMN_INDEX,
					 jsonbData->valuesTable);
}

static void
PGrnJSONBCreateIndexColumns(PGrnCreateData *data,
							PGrnJSONBCreateData *jsonbData)
{
	PGrnCreateColumn(data->index,
					 jsonbData->valuesTable,
					 PGrnIndexColumnName,
					 GRN_OBJ_COLUMN_INDEX,
					 data->sourcesTable);
	PGrnCreateColumn(data->index,
					 jsonbData->pathsTable,
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

bool
PGrnAttributeIsJSONB(Oid id)
{
	return id == JSONBOID;
}

static bool
PGrnJSONBIsForFullTextSearchOnly(Relation index, unsigned int nthAttribute)
{
	Oid strategyOID;

	strategyOID = get_opfamily_member(index->rd_opfamily[nthAttribute],
									  JSONBOID,
									  TEXTOID,
									  PGrnScriptStrategyV2Number);
	return !OidIsValid(strategyOID);
}

void
PGrnJSONBCreate(PGrnCreateData *data)
{
	const char *tag = "[jsonb][create]";

	if (data->desc->natts != 1)
	{
		PGrnCheckRC(GRN_FUNCTION_NOT_IMPLEMENTED,
					"%s multicolumn index isn't supported: <%s>",
					tag,
					data->index->rd_rel->relname.data);
	}

	if (PGrnJSONBIsForFullTextSearchOnly(data->index, data->i))
	{
		data->forFullTextSearch = true;
		data->attributeTypeID = GRN_DB_TEXT;
		data->attributeFlags = GRN_OBJ_VECTOR;
		PGrnCreateLexicon(data);
		PGrnCreateDataColumn(data);
		PGrnCreateIndexColumn(data);
	}
	else
	{
		PGrnJSONBCreateData jsonbData;
		PGrnJSONBCreateTables(data, &jsonbData);
		PGrnJSONBCreateDataColumns(data->index, &jsonbData);
		PGrnJSONBCreateIndexColumns(data, &jsonbData);
		data->attributeTypeID = grn_obj_id(ctx, jsonbData.valuesTable);
		data->attributeFlags = GRN_OBJ_VECTOR;
		PGrnCreateDataColumn(data);
	}
}

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

	snprintf(indexName,
			 sizeof(indexName),
			 PGrnJSONValueLexiconNameFormat ".%s",
			 typeName,
			 PGRN_RELATION_GET_LOCATOR_NUMBER(index),
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
	PGrnIndexColumnSetSource(index, indexColumn, source);

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
		grn_obj *sourceIDs;

		sourceIDs = &(buffers->sourceIDs);
		GRN_BULK_REWIND(sourceIDs);

		source = PGrnLookupColumn(jsonValuesTable, "path", ERROR);
		GRN_UINT32_PUT(ctx, sourceIDs, grn_obj_id(ctx, source));
		grn_obj_unlink(ctx, source);

		source = PGrnLookupColumn(jsonValuesTable, "paths", ERROR);
		GRN_UINT32_PUT(ctx, sourceIDs, grn_obj_id(ctx, source));
		grn_obj_unlink(ctx, source);

		indexColumn =
			PGrnLookupColumn(jsonPathsTable, PGrnIndexColumnName, ERROR);
		PGrnIndexColumnSetSourceIDs(index, indexColumn, sourceIDs);
	}

	PGrnJSONBValueSetSource(
		index, jsonValuesTable, "string", "String", nthAttribute, true);
	PGrnJSONBValueSetSource(
		index, jsonValuesTable, "number", "Number", nthAttribute, true);
	PGrnJSONBValueSetSource(
		index, jsonValuesTable, "boolean", "Boolean", nthAttribute, true);
	PGrnJSONBValueSetSource(
		index, jsonValuesTable, "size", "Size", nthAttribute, true);
	PGrnJSONBValueSetSource(index,
							jsonValuesTable,
							"string",
							"FullTextSearch",
							nthAttribute,
							false);

	grn_obj_unlink(ctx, jsonPathsTable);
}

grn_obj *
PGrnJSONBSetSource(Relation index, unsigned int i)
{
	grn_obj *indexColumn;

	if (PGrnJSONBIsForFullTextSearchOnly(index, i))
	{
		indexColumn = PGrnLookupIndexColumn(index, i, ERROR);
	}
	else
	{
		grn_obj *jsonValuesTable;
		jsonValuesTable = PGrnJSONBLookupValuesTable(index, i, ERROR);
		PGrnJSONBSetSources(index, jsonValuesTable, i);
		indexColumn =
			PGrnLookupColumn(jsonValuesTable, PGrnIndexColumnName, ERROR);
	}

	return indexColumn;
}

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

static bool
PGrnJSONBMatchExpression(Jsonb *target,
						 const char *term,
						 unsigned int termSize,
						 const char *query,
						 unsigned int querySize,
						 const char *script,
						 unsigned int scriptSize,
						 const char *logTag)
{
	grn_obj valueIDs;
	grn_obj tokenStack;
	PGrnJSONBInsertData data;
	JsonbIterator *iter;
	grn_obj *matchTarget = NULL;
	grn_obj *condition = NULL;
	grn_obj *dummyVariable = NULL;
	grn_obj *result = NULL;
	bool matched = false;

	if (termSize == 0 && querySize == 0 && scriptSize == 0)
		return false;

	data.index = NULL;
	data.isForFullTextSearchOnly = false;
	data.pathsTable = tmpPathsTable;
	data.valuesTable = tmpValuesTable;
	GRN_PTR_INIT(&valueIDs, GRN_OBJ_VECTOR, grn_obj_id(ctx, data.valuesTable));
	data.valueIDs = &valueIDs;
	GRN_UINT8_INIT(&tokenStack, GRN_OBJ_VECTOR);
	data.tokenStack = &tokenStack;
	PGrnJSONBInsertDataInit(&data);
	iter = JsonbIteratorInit(&(target->root));
	PGrnJSONBInsertContainer(&iter, &data);
	PGrnJSONBInsertDataFin(&data);

	PG_TRY();
	{
		GRN_EXPR_CREATE_FOR_QUERY(
			ctx, tmpValuesTable, condition, dummyVariable);
		PGrnCheck("%s: failed to create condition expression object", logTag);
		if (termSize > 0)
		{
			const char *targetName = "string";
			grn_obj *column;

			column = grn_obj_column(
				ctx, tmpValuesTable, targetName, strlen(targetName));
			grn_expr_append_obj(ctx, condition, column, GRN_OP_GET_VALUE, 1);
			PGrnCheck("%s: failed to append column: %s", logTag, targetName);
			grn_expr_append_const_str(
				ctx, condition, term, termSize, GRN_OP_PUSH, 1);
			PGrnCheck("%s: failed to append text: <%.*s>",
					  logTag,
					  (int) termSize,
					  term);
			PGrnExprAppendOp(condition, GRN_OP_MATCH, 2, logTag, NULL);
		}
		else if (querySize > 0)
		{
			const char *matchColumns = "string";

			GRN_EXPR_CREATE_FOR_QUERY(
				ctx, tmpValuesTable, matchTarget, dummyVariable);
			PGrnCheck("%s: failed to create match target expression object",
					  logTag);
			grn_expr_parse(ctx,
						   matchTarget,
						   matchColumns,
						   strlen(matchColumns),
						   NULL,
						   GRN_OP_MATCH,
						   GRN_OP_AND,
						   GRN_EXPR_SYNTAX_SCRIPT);
			PGrnCheck("%s: failed to parse match columns: <%.*s>",
					  logTag,
					  (int) strlen(matchColumns),
					  matchColumns);
			grn_expr_parse(ctx,
						   condition,
						   query,
						   querySize,
						   matchTarget,
						   GRN_OP_MATCH,
						   GRN_OP_AND,
						   GRN_EXPR_SYNTAX_QUERY);
			PGrnCheck("%s: failed to parse query: <%.*s>",
					  logTag,
					  (int) querySize,
					  query);
		}
		else if (scriptSize > 0)
		{
			grn_expr_parse(ctx,
						   condition,
						   script,
						   scriptSize,
						   NULL,
						   GRN_OP_MATCH,
						   GRN_OP_AND,
						   GRN_EXPR_SYNTAX_SCRIPT);
			PGrnCheck("%s: failed to parse script: <%.*s>",
					  logTag,
					  (int) scriptSize,
					  script);
		}
		result = grn_table_create(ctx,
								  NULL,
								  0,
								  NULL,
								  GRN_TABLE_HASH_KEY | GRN_OBJ_WITH_SUBREC,
								  tmpValuesTable,
								  NULL);
		PGrnCheck("%s: failed to create result table", logTag);
		grn_table_select(ctx, tmpValuesTable, condition, result, GRN_OP_OR);
		PGrnCheck("%s: failed to select", logTag);
	}
	PG_CATCH();
	{
		if (result)
			grn_obj_close(ctx, result);
		if (condition)
			grn_obj_close(ctx, condition);
		if (matchTarget)
			grn_obj_close(ctx, matchTarget);
		GRN_OBJ_FIN(ctx, &tokenStack);
		PGrnJSONBDeleteValues(tmpValuesTable, &valueIDs);
		GRN_OBJ_FIN(ctx, &valueIDs);
		PG_RE_THROW();
	}
	PG_END_TRY();

	matched = grn_table_size(ctx, result) > 0;

	grn_obj_close(ctx, result);
	grn_obj_close(ctx, condition);
	if (matchTarget)
		grn_obj_close(ctx, matchTarget);

	PGrnJSONBDeleteValues(tmpValuesTable, &valueIDs);
	GRN_OBJ_FIN(ctx, &valueIDs);

	return matched;
}

/**
 * pgroonga.match_script_jsonb(jsonb, query) : bool
 */
Datum
pgroonga_match_script_jsonb(PG_FUNCTION_ARGS)
{
	Jsonb *target = PG_GETARG_JSONB_P(0);
	text *script = PG_GETARG_TEXT_PP(1);
	bool matched;

	matched = PGrnJSONBMatchExpression(target,
									   NULL,
									   0,
									   NULL,
									   0,
									   VARDATA_ANY(script),
									   VARSIZE_ANY_EXHDR(script),
									   "jsonb: script");
	PG_RETURN_BOOL(matched);
}

/**
 * pgroonga.match_jsonb(target jsonb, term text) : bool
 */
Datum
pgroonga_match_jsonb(PG_FUNCTION_ARGS)
{
	Jsonb *target = PG_GETARG_JSONB_P(0);
	text *term = PG_GETARG_TEXT_PP(1);
	bool matched;

	matched = PGrnJSONBMatchExpression(target,
									   VARDATA_ANY(term),
									   VARSIZE_ANY_EXHDR(term),
									   NULL,
									   0,
									   NULL,
									   0,
									   "jsonb: match");
	PG_RETURN_BOOL(matched);
}

/**
 * pgroonga.query_jsonb(target jsonb, query text) : bool
 */
Datum
pgroonga_query_jsonb(PG_FUNCTION_ARGS)
{
	Jsonb *target = PG_GETARG_JSONB_P(0);
	text *query = PG_GETARG_TEXT_PP(1);
	bool matched;

	matched = PGrnJSONBMatchExpression(target,
									   NULL,
									   0,
									   VARDATA_ANY(query),
									   VARSIZE_ANY_EXHDR(query),
									   NULL,
									   0,
									   "jsonb: query");
	PG_RETURN_BOOL(matched);
}

/**
 * pgroonga.script_jsonb(target jsonb, script text) : bool
 */
Datum
pgroonga_script_jsonb(PG_FUNCTION_ARGS)
{
	Jsonb *target = PG_GETARG_JSONB_P(0);
	text *script = PG_GETARG_TEXT_PP(1);
	bool matched;

	matched = PGrnJSONBMatchExpression(target,
									   NULL,
									   0,
									   NULL,
									   0,
									   VARDATA_ANY(script),
									   VARSIZE_ANY_EXHDR(script),
									   "jsonb: script");
	PG_RETURN_BOOL(matched);
}

static void
PGrnJSONBInsertJSONB(PGrnJSONBInsertData *data, Jsonb *jsonb)
{
	JsonbIterator *iter;

	iter = JsonbIteratorInit(&(jsonb->root));
	if (data->isForFullTextSearchOnly)
		PGrnJSONBInsertContainerForFullTextSearch(&iter, data);
	else
		PGrnJSONBInsertContainer(&iter, data);
}

static void
PGrnJSONBInsertRecord(Relation index,
					  grn_obj *sourcesTable,
					  grn_obj *sourcesCtidColumn,
					  uint64_t packedCtid,
					  PGrnJSONBInsertData *data)
{
	TupleDesc desc = RelationGetDescr(index);
	grn_id id;
	PGrnWALData *walData;
	Form_pg_attribute attribute;
	NameData *attributeName;
	grn_obj *column;

	walData = PGrnWALStart(index);
	PGrnWALInsertStart(walData, sourcesTable, desc->natts + 1);
	if (sourcesCtidColumn)
	{
		id = grn_table_add(ctx, sourcesTable, NULL, 0, NULL);
		GRN_UINT64_SET(ctx, &(buffers->ctid), packedCtid);
		grn_obj_set_value(
			ctx, sourcesCtidColumn, id, &(buffers->ctid), GRN_OBJ_SET);
		PGrnWALInsertColumn(walData, sourcesCtidColumn, &(buffers->ctid));
	}
	else
	{
		id = grn_table_add(
			ctx, sourcesTable, &packedCtid, sizeof(uint64_t), NULL);
		PGrnWALInsertKeyRaw(walData, &packedCtid, sizeof(uint64_t));
	}

	attribute = TupleDescAttr(desc, 0);
	attributeName = &(attribute->attname);
	column = PGrnLookupColumn(sourcesTable, attributeName->data, ERROR);
	if (data->isForFullTextSearchOnly)
		grn_obj_set_value(ctx, column, id, &(data->value), GRN_OBJ_SET);
	else
		grn_obj_set_value(ctx, column, id, data->valueIDs, GRN_OBJ_SET);
	PGrnCheck("failed to set column value: <%s>", attributeName->data);

	if (walData)
	{
		if (data->isForFullTextSearchOnly)
		{
			PGrnWALInsertColumn(walData, column, &(data->value));
		}
		else
		{
			grn_obj *valueKeys = &(buffers->jsonbValueKeys);
			size_t i, nValueIDs;

			GRN_BULK_REWIND(valueKeys);
			nValueIDs = GRN_BULK_VSIZE(data->valueIDs) / sizeof(grn_id);
			for (i = 0; i < nValueIDs; i++)
			{
				grn_id valueID;
				uint64_t key;

				valueID = GRN_RECORD_VALUE_AT(data->valueIDs, i);
				grn_table_get_key(
					ctx, data->valuesTable, valueID, &key, sizeof(uint64_t));
				GRN_UINT64_PUT(ctx, valueKeys, key);
			}
			PGrnWALInsertColumn(walData, column, valueKeys);
		}
	}

	PGrnWALInsertFinish(walData);
	PGrnWALFinish(walData);
}

uint32_t
PGrnJSONBInsert(Relation index,
				grn_obj *sourcesTable,
				grn_obj *sourcesCtidColumn,
				Datum *values,
				bool *isnull,
				uint64_t packedCtid)
{
	uint32_t recordSize = 0; /* always 0 */
	PGrnJSONBInsertData data;
	unsigned int nthAttribute = 0;

	data.index = index;
	data.isForFullTextSearchOnly =
		PGrnJSONBIsForFullTextSearchOnly(index, nthAttribute);
	if (!data.isForFullTextSearchOnly)
	{
		data.pathsTable = PGrnJSONBLookupPathsTable(index, nthAttribute, ERROR);
		data.valuesTable =
			PGrnJSONBLookupValuesTable(index, nthAttribute, ERROR);
		data.valueIDs = &(buffers->general);
		grn_obj_reinit(ctx,
					   data.valueIDs,
					   grn_obj_id(ctx, data.valuesTable),
					   GRN_OBJ_VECTOR);
		data.tokenStack = &(buffers->jsonbTokenStack);
	}
	PGrnJSONBInsertDataInit(&data);

	if (!isnull[nthAttribute])
	{
		Jsonb *jsonb = DatumGetJsonbP(values[nthAttribute]);
		PGrnJSONBInsertJSONB(&data, jsonb);
	}

	PGrnJSONBInsertRecord(
		index, sourcesTable, sourcesCtidColumn, packedCtid, &data);

	PGrnJSONBInsertDataFin(&data);

	return recordSize;
}

static void
PGrnSearchBuildConditionJSONScript(PGrnSearchData *data,
								   grn_obj *subFilter,
								   grn_obj *targetColumn,
								   grn_obj *filter,
								   unsigned int *nthCondition)
{
	const char *tag = "jsonb: [build-condition][json-script]";

	grn_expr_append_obj(ctx, data->expression, subFilter, GRN_OP_PUSH, 1);
	PGrnCheck("%s: failed to append sub_filter()", tag);
	grn_expr_append_obj(ctx, data->expression, targetColumn, GRN_OP_PUSH, 1);
	PGrnCheck(
		"%s: failed to append column: %s", tag, PGrnInspectName(targetColumn));
	grn_expr_append_const(ctx, data->expression, filter, GRN_OP_PUSH, 1);
	PGrnCheck("%s: failed to append filter: <%.*s>",
			  tag,
			  (int) GRN_TEXT_LEN(filter),
			  GRN_TEXT_VALUE(filter));
	PGrnExprAppendOp(data->expression, GRN_OP_CALL, 2, tag, NULL);

	if (*nthCondition > 0)
	{
		PGrnExprAppendOp(data->expression, GRN_OP_AND, 2, tag, NULL);
	}

	(*nthCondition)++;
}

static void
PGrnSearchBuildConditionJSONMatch(PGrnSearchData *data,
								  grn_obj *subFilter,
								  grn_obj *targetColumn,
								  grn_obj *term)
{
	unsigned int nthCondition = 0;

	GRN_BULK_REWIND(&(buffers->general));

	GRN_TEXT_PUTS(ctx, &(buffers->general), "string @ ");
	grn_text_esc(
		ctx, &(buffers->general), GRN_TEXT_VALUE(term), GRN_TEXT_LEN(term));

	PGrnSearchBuildConditionJSONScript(
		data, subFilter, targetColumn, &(buffers->general), &nthCondition);
}

static void
PGrnSearchBuildConditionJSONQuery(PGrnSearchData *data,
								  grn_obj *subFilter,
								  grn_obj *targetColumn,
								  grn_obj *query)
{
	unsigned int nthCondition = 0;

	GRN_BULK_REWIND(&(buffers->general));

	GRN_TEXT_PUTS(ctx, &(buffers->general), "query(\"string\", ");
	grn_text_esc(
		ctx, &(buffers->general), GRN_TEXT_VALUE(query), GRN_TEXT_LEN(query));
	GRN_TEXT_PUTS(ctx, &(buffers->general), ")");

	PGrnSearchBuildConditionJSONScript(
		data, subFilter, targetColumn, &(buffers->general), &nthCondition);
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
	grn_text_esc(ctx,
				 &(buffers->general),
				 GRN_TEXT_VALUE(&(buffers->path)),
				 GRN_TEXT_LEN(&(buffers->path)));

	PGrnSearchBuildConditionJSONScript(
		data, subFilter, targetColumn, &(buffers->general), nthCondition);
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
		grn_text_esc(ctx,
					 &(buffers->general),
					 value->val.string.val,
					 value->val.string.len);
		break;
	case jbvNumeric:
	{
		Datum numericInString = DirectFunctionCall1(
			numeric_out, NumericGetDatum(value->val.numeric));
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
	grn_text_esc(ctx,
				 &(buffers->general),
				 GRN_TEXT_VALUE(&(buffers->path)),
				 GRN_TEXT_LEN(&(buffers->path)));

	PGrnSearchBuildConditionJSONScript(
		data, subFilter, targetColumn, &(buffers->general), nthCondition);
}

static void
PGrnSearchBuildConditionJSONContain(PGrnSearchData *data,
									grn_obj *subFilter,
									grn_obj *targetColumn,
									Jsonb *jsonb)
{
	const char *tag = "[build-condition][json-contain]";
	unsigned int nthCondition = 0;
	grn_obj components;
	JsonbIterator *iter;
	JsonbIteratorToken token;
	JsonbValue value;

	GRN_TEXT_INIT(&components, GRN_OBJ_VECTOR);
	iter = JsonbIteratorInit(&(jsonb->root));
	while ((token = JsonbIteratorNext(&iter, &value, false)) != WJB_DONE)
	{
		switch (token)
		{
		case WJB_KEY:
			grn_vector_add_element(ctx,
								   &components,
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
			grn_vector_add_element(ctx,
								   &components,
								   (const char *) &nElements,
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
			grn_vector_pop_element(ctx, &components, &component, NULL, NULL);
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
			PGrnCheckRC(GRN_UNKNOWN_ERROR,
						"%s iterator returns invalid token: %s",
						tag,
						PGrnJSONBIteratorTokenToString(token));
			break;
		}
	}
	GRN_OBJ_FIN(ctx, &components);
}

bool
PGrnJSONBBuildSearchCondition(PGrnSearchData *data,
							  Relation index,
							  ScanKey key,
							  grn_obj *targetColumn)
{
	const char *tag = "[build-condition][jsonb]";
	unsigned int nthAttribute = 0;
	grn_obj *subFilter;

	subFilter = PGrnLookup("sub_filter", ERROR);
	grn_obj_reinit(ctx, &(buffers->general), GRN_DB_TEXT, 0);

	switch (key->sk_strategy)
	{
	case PGrnQueryStrategyNumber: /* For backward compatibility */
	case PGrnScriptStrategyV2Number:
	{
		unsigned int nthCondition = 0;
		PGrnConvertFromData(key->sk_argument, TEXTOID, &(buffers->general));
		PGrnSearchBuildConditionJSONScript(
			data, subFilter, targetColumn, &(buffers->general), &nthCondition);
		break;
	}
	case PGrnMatchStrategyV2Number:
		grn_obj_reinit(ctx, &(buffers->keyword), GRN_DB_TEXT, 0);
		PGrnConvertFromData(key->sk_argument, TEXTOID, &(buffers->keyword));
		if (PGrnJSONBIsForFullTextSearchOnly(index, nthAttribute))
		{
			PGrnSearchBuildConditionBinaryOperation(
				data, targetColumn, &(buffers->keyword), GRN_OP_MATCH);
		}
		else
		{
			PGrnSearchBuildConditionJSONMatch(
				data, subFilter, targetColumn, &(buffers->keyword));
		}
		break;
	case PGrnQueryStrategyV2Number:
	case PGrnQueryStrategyV2DeprecatedNumber:
		grn_obj_reinit(ctx, &(buffers->keyword), GRN_DB_TEXT, 0);
		PGrnConvertFromData(key->sk_argument, TEXTOID, &(buffers->keyword));
		if (PGrnJSONBIsForFullTextSearchOnly(index, nthAttribute))
		{
			PGrnSearchBuildConditionQuery(data,
										  targetColumn,
										  GRN_TEXT_VALUE(&(buffers->keyword)),
										  GRN_TEXT_LEN(&(buffers->keyword)));
		}
		else
		{
			PGrnSearchBuildConditionJSONQuery(
				data, subFilter, targetColumn, &(buffers->keyword));
		}
		break;
	case PGrnContainStrategyNumber:
		PGrnSearchBuildConditionJSONContain(
			data, subFilter, targetColumn, DatumGetJsonbP(key->sk_argument));
		break;
	default:
		PGrnCheckRC(GRN_INVALID_ARGUMENT,
					"%s unexpected strategy number: %d",
					tag,
					key->sk_strategy);
		break;
	}
	return true;
}

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
PGrnJSONValuesDeleteBulk(PGrnJSONBBulkDeleteData *data)
{
	grn_obj *jsonValuesTable = data->valuesTable;
	grn_obj *jsonValuesIndexColumn = data->valuesIndexColumn;
	grn_obj *valueMin = &(data->valueMin);
	grn_obj *valueMax = &(data->valueMax);
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
	tableCursor = grn_table_cursor_open(
		ctx, jsonValuesTable, minKey, minKeySize, maxKey, maxKeySize, 0, -1, 0);
	if (!tableCursor)
		return;

	while ((valueID = grn_table_cursor_next(ctx, tableCursor)) != GRN_ID_NIL)
	{
		grn_ii_cursor *iiCursor;
		bool haveReference = false;

		iiCursor = grn_ii_cursor_open(ctx,
									  (grn_ii *) jsonValuesIndexColumn,
									  valueID,
									  GRN_ID_NIL,
									  GRN_ID_MAX,
									  0,
									  0);
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
		{
			void *key;
			int keySize;
			keySize = grn_table_cursor_get_key(ctx, tableCursor, &key);
			PGrnWALDelete(data->index, jsonValuesTable, key, keySize);
			grn_table_cursor_delete(ctx, tableCursor);
		}
	}

	grn_table_cursor_close(ctx, tableCursor);
}

void
PGrnJSONBBulkDeleteInit(PGrnJSONBBulkDeleteData *data)
{
	Relation index = data->index;
	unsigned int nthAttribute = 0;
	TupleDesc desc;
	Form_pg_attribute attribute;
	grn_id valuesTableID;

	desc = RelationGetDescr(index);
	attribute = TupleDescAttr(desc, nthAttribute);
	data->isJSONBAttribute = PGrnAttributeIsJSONB(attribute->atttypid);
	if (!data->isJSONBAttribute)
		return;

	data->isForFullTextSearchOnly =
		PGrnJSONBIsForFullTextSearchOnly(index, nthAttribute);
	if (data->isForFullTextSearchOnly)
		return;

	data->sourcesValuesColumn =
		PGrnLookupColumn(data->sourcesTable, attribute->attname.data, ERROR);
	data->valuesTable = PGrnJSONBLookupValuesTable(index, 0, ERROR);
	data->valuesIndexColumn =
		PGrnLookupColumn(data->valuesTable, PGrnIndexColumnName, ERROR);

	valuesTableID = grn_obj_id(ctx, data->valuesTable);
	GRN_RECORD_INIT(&(data->values), 0, valuesTableID);
	GRN_RECORD_INIT(&(data->valueMin), 0, valuesTableID);
	GRN_RECORD_INIT(&(data->valueMax), 0, valuesTableID);

	GRN_RECORD_SET(ctx, &(data->valueMin), GRN_ID_NIL);
	GRN_RECORD_SET(ctx, &(data->valueMax), GRN_ID_NIL);
}

void
PGrnJSONBBulkDeleteRecord(PGrnJSONBBulkDeleteData *data)
{
	if (!data->isJSONBAttribute)
		return;

	if (data->isForFullTextSearchOnly)
		return;

	GRN_BULK_REWIND(&(data->values));
	grn_obj_get_value(
		ctx, data->sourcesValuesColumn, data->id, &(data->values));
	PGrnJSONValuesUpdateDeletedID(data->valuesTable,
								  &(data->values),
								  &(data->valueMin),
								  &(data->valueMax));
}

void
PGrnJSONBBulkDeleteFin(PGrnJSONBBulkDeleteData *data)
{
	if (!data->isJSONBAttribute)
		return;

	if (data->isForFullTextSearchOnly)
		return;

	PGrnJSONValuesDeleteBulk(data);

	GRN_OBJ_FIN(ctx, &(data->values));
	grn_obj_unlink(ctx, data->sourcesValuesColumn);
	grn_obj_unlink(ctx, data->valuesIndexColumn);
	grn_obj_unlink(ctx, data->valuesTable);
}

static void
PGrnRemoveJSONValueLexicon(const char *typeName, unsigned int relationID)
{
	char tableName[GRN_TABLE_MAX_KEY_SIZE];
	snprintf(tableName,
			 sizeof(tableName),
			 PGrnJSONValueLexiconNameFormat,
			 typeName,
			 relationID,
			 0);
	if (!grn_ctx_get(ctx, tableName, -1))
		return;
	PGrnRemoveObject(tableName);
}

void
PGrnJSONBRemoveUnusedTables(Oid relationFileNodeID)
{
	{
		char jsonValuesTableName[GRN_TABLE_MAX_KEY_SIZE];
		snprintf(jsonValuesTableName,
				 sizeof(jsonValuesTableName),
				 PGrnJSONValuesTableNameFormat,
				 relationFileNodeID,
				 0);
		if (!grn_ctx_get(ctx, jsonValuesTableName, -1))
			return;
	}

	PGrnRemoveJSONValueLexicon("FullTextSearch", relationFileNodeID);
	PGrnRemoveJSONValueLexicon("String", relationFileNodeID);
	PGrnRemoveJSONValueLexicon("Number", relationFileNodeID);
	PGrnRemoveJSONValueLexicon("Boolean", relationFileNodeID);
	PGrnRemoveJSONValueLexicon("Size", relationFileNodeID);

	{
		char name[GRN_TABLE_MAX_KEY_SIZE];

		snprintf(name,
				 sizeof(name),
				 PGrnJSONPathsTableNameFormat ".%s",
				 relationFileNodeID,
				 0,
				 PGrnIndexColumnName);
		PGrnRemoveObject(name);

		snprintf(name,
				 sizeof(name),
				 PGrnJSONValuesTableNameFormat,
				 relationFileNodeID,
				 0);
		PGrnRemoveObject(name);

		snprintf(name,
				 sizeof(name),
				 PGrnJSONPathsTableNameFormat,
				 relationFileNodeID,
				 0);
		PGrnRemoveObject(name);

		snprintf(name,
				 sizeof(name),
				 PGrnJSONTypesTableNameFormat,
				 relationFileNodeID,
				 0);
		PGrnRemoveObject(name);
	}
}
