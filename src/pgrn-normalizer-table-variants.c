#include "pgroonga.h"

#include "pgrn-groonga.h"
#include "pgrn-jsonb.h"
#include "pgrn-pg.h"

#include <catalog/pg_type.h>
#include <utils/array.h>
#include <utils/builtins.h>

#include <string.h>

typedef struct
{
	char *target;
	size_t targetSize;
	char *normalized;
	size_t normalizedSize;
} PGrnNormalizerTableVariant;

typedef struct
{
	PGrnNormalizerTableVariant *items;
	size_t nItems;
	size_t maxNItems;
} PGrnNormalizerTableVariants;

typedef struct
{
	char **items;
	size_t *sizes;
	size_t nItems;
	size_t maxNItems;
} PGrnNormalizerTableVariantResults;

PGDLLEXPORT PG_FUNCTION_INFO_V1(pgroonga_normalizer_table_variants);

static void
PGrnNormalizerTableVariantResultsAppend(PGrnNormalizerTableVariantResults *results,
										const char *variant,
										size_t variantSize)
{
	size_t i;

	for (i = 0; i < results->nItems; i++)
	{
		if (results->sizes[i] == variantSize &&
			memcmp(results->items[i], variant, variantSize) == 0)
		{
			return;
		}
	}

	if (results->nItems == results->maxNItems)
	{
		if (results->maxNItems == 0)
		{
			results->maxNItems = 8;
			results->items = palloc(sizeof(char *) * results->maxNItems);
			results->sizes = palloc(sizeof(size_t) * results->maxNItems);
		}
		else
		{
			results->maxNItems *= 2;
			results->items =
				repalloc(results->items, sizeof(char *) * results->maxNItems);
			results->sizes =
				repalloc(results->sizes, sizeof(size_t) * results->maxNItems);
		}
	}

	results->items[results->nItems] = pnstrdup(variant, variantSize);
	results->sizes[results->nItems] = variantSize;
	results->nItems++;
}

static void
PGrnNormalizerTableVariantsAppend(PGrnNormalizerTableVariants *variants,
								  const char *target,
								  size_t targetSize,
								  const char *normalized,
								  size_t normalizedSize)
{
	if (variants->nItems == variants->maxNItems)
	{
		if (variants->maxNItems == 0)
		{
			variants->maxNItems = 8;
			variants->items = palloc(sizeof(PGrnNormalizerTableVariant) *
									 variants->maxNItems);
		}
		else
		{
			variants->maxNItems *= 2;
			variants->items = repalloc(variants->items,
									   sizeof(PGrnNormalizerTableVariant) *
										   variants->maxNItems);
		}
	}

	variants->items[variants->nItems].target = pnstrdup(target, targetSize);
	variants->items[variants->nItems].targetSize = targetSize;
	variants->items[variants->nItems].normalized =
		pnstrdup(normalized, normalizedSize);
	variants->items[variants->nItems].normalizedSize = normalizedSize;
	variants->nItems++;
}

static bool
PGrnNormalizerTableParseQuotedArgument(const char **current,
									   const char *end,
									   char **value,
									   size_t *valueSize)
{
	const char *start;
	grn_obj buffer;

	while (*current < end && **current != '"')
	{
		(*current)++;
	}
	if (*current == end)
		return false;

	(*current)++;
	start = *current;
	GRN_TEXT_INIT(&buffer, 0);
	while (*current < end)
	{
		if (**current == '\\')
		{
			GRN_TEXT_PUT(ctx, &buffer, start, *current - start);
			(*current)++;
			if (*current == end)
				break;
			GRN_TEXT_PUT(ctx, &buffer, *current, 1);
			(*current)++;
			start = *current;
		}
		else if (**current == '"')
		{
			GRN_TEXT_PUT(ctx, &buffer, start, *current - start);
			*value = pnstrdup(GRN_TEXT_VALUE(&buffer), GRN_TEXT_LEN(&buffer));
			*valueSize = GRN_TEXT_LEN(&buffer);
			GRN_OBJ_FIN(ctx, &buffer);
			(*current)++;
			return true;
		}
		else
		{
			(*current)++;
		}
	}

	GRN_OBJ_FIN(ctx, &buffer);
	return false;
}

static bool
PGrnNormalizerTableParse(const char *normalizers,
						 size_t normalizersSize,
						 char **normalizedColumnObjectName,
						 size_t *normalizedColumnObjectNameSize,
						 char **targetColumnName,
						 size_t *targetColumnNameSize)
{
	const char normalizerTable[] = "NormalizerTable";
	const size_t normalizerTableSize = sizeof(normalizerTable) - 1;
	const char *current = normalizers;
	const char *end = normalizers + normalizersSize;
	char *arguments[4] = {NULL, NULL, NULL, NULL};
	size_t argumentSizes[4] = {0, 0, 0, 0};
	size_t i;

	while (current + normalizerTableSize < end)
	{
		if (memcmp(current, normalizerTable, normalizerTableSize) == 0)
			break;
		current++;
	}
	if (current + normalizerTableSize >= end)
		return false;

	current += normalizerTableSize;
	while (current < end && *current != '(')
		current++;
	if (current == end)
		return false;
	current++;

	for (i = 0; i < 4; i++)
	{
		if (!PGrnNormalizerTableParseQuotedArgument(
				&current, end, &(arguments[i]), &(argumentSizes[i])))
		{
			return false;
		}
	}

	*normalizedColumnObjectName = arguments[1];
	*normalizedColumnObjectNameSize = argumentSizes[1];
	*targetColumnName = arguments[3];
	*targetColumnNameSize = argumentSizes[3];
	return true;
}

static int
PGrnNormalizerTableResolveLexiconIndex(Relation index)
{
	int i;

	for (i = 0; i < index->rd_att->natts; i++)
	{
		grn_obj *lexicon = PGrnLookupLexicon(index, i, ERROR);
		bool isTextLexicon =
			grn_type_id_is_text_family(ctx, lexicon->header.domain);
		grn_obj_unref(ctx, lexicon);
		if (isTextLexicon)
			return i;
	}

	ereport(ERROR,
			(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			 errmsg("pgroonga: [normalizer-table-variants] "
					"index doesn't have a lexicon for text: <%s>",
					RelationGetRelationName(index))));
	return -1;
}

static void
PGrnNormalizerTableLoadVariants(PGrnNormalizerTableVariants *variants,
								grn_obj *normalizationTable,
								grn_obj *normalizedColumn,
								grn_obj *targetColumn)
{
	grn_table_cursor *cursor;
	grn_id id;
	grn_id normalizedRangeID;
	grn_id targetRangeID;
	grn_obj normalizedValue;
	grn_obj targetValue;
	grn_obj normalizedText;
	grn_obj targetText;

	normalizedRangeID = grn_obj_get_range(ctx, normalizedColumn);
	targetRangeID = grn_obj_get_range(ctx, targetColumn);
	GRN_VOID_INIT(&normalizedValue);
	GRN_VOID_INIT(&targetValue);
	grn_obj_reinit(ctx, &normalizedValue, normalizedRangeID, 0);
	grn_obj_reinit(ctx, &targetValue, targetRangeID, 0);
	GRN_TEXT_INIT(&normalizedText, 0);
	GRN_TEXT_INIT(&targetText, 0);

	cursor = grn_table_cursor_open(
		ctx, normalizationTable, NULL, 0, NULL, 0, 0, -1, 0);
	if (!cursor)
	{
		PGrnCheckRC(GRN_NO_MEMORY_AVAILABLE,
					"[normalizer-table-variants] failed to create cursor");
	}

	while ((id = grn_table_cursor_next(ctx, cursor)) != GRN_ID_NIL)
	{
		GRN_BULK_REWIND(&normalizedValue);
		GRN_BULK_REWIND(&targetValue);
		GRN_BULK_REWIND(&normalizedText);
		GRN_BULK_REWIND(&targetText);
		grn_obj_get_value(ctx, normalizedColumn, id, &normalizedValue);
		grn_obj_get_value(ctx, targetColumn, id, &targetValue);
		if (grn_type_id_is_text_family(ctx, normalizedRangeID))
		{
			GRN_TEXT_PUT(ctx,
						 &normalizedText,
						 GRN_TEXT_VALUE(&normalizedValue),
						 GRN_TEXT_LEN(&normalizedValue));
		}
		else if (grn_id_maybe_table(ctx, normalizedRangeID))
		{
			grn_obj *rangeTable = grn_ctx_at(ctx, normalizedRangeID);
			char key[GRN_TABLE_MAX_KEY_SIZE];
			int keySize = grn_table_get_key(
				ctx, rangeTable, GRN_RECORD_VALUE(&normalizedValue), key, sizeof(key));
			GRN_TEXT_PUT(ctx, &normalizedText, key, keySize);
			grn_obj_unref(ctx, rangeTable);
		}
		if (grn_type_id_is_text_family(ctx, targetRangeID))
		{
			GRN_TEXT_PUT(ctx,
						 &targetText,
						 GRN_TEXT_VALUE(&targetValue),
						 GRN_TEXT_LEN(&targetValue));
		}
		else if (grn_id_maybe_table(ctx, targetRangeID))
		{
			grn_obj *rangeTable = grn_ctx_at(ctx, targetRangeID);
			char key[GRN_TABLE_MAX_KEY_SIZE];
			int keySize = grn_table_get_key(
				ctx, rangeTable, GRN_RECORD_VALUE(&targetValue), key, sizeof(key));
			GRN_TEXT_PUT(ctx, &targetText, key, keySize);
			grn_obj_unref(ctx, rangeTable);
		}
		if (GRN_TEXT_LEN(&normalizedText) == 0 || GRN_TEXT_LEN(&targetText) == 0)
		{
			continue;
		}
		PGrnNormalizerTableVariantsAppend(variants,
										  GRN_TEXT_VALUE(&targetText),
										  GRN_TEXT_LEN(&targetText),
										  GRN_TEXT_VALUE(&normalizedText),
										  GRN_TEXT_LEN(&normalizedText));
	}

	grn_table_cursor_close(ctx, cursor);
	GRN_OBJ_FIN(ctx, &normalizedValue);
	GRN_OBJ_FIN(ctx, &targetValue);
	GRN_OBJ_FIN(ctx, &normalizedText);
	GRN_OBJ_FIN(ctx, &targetText);
}

static void
PGrnNormalizerTableVariantsExpand(PGrnNormalizerTableVariants *variants,
								  const char *normalized,
								  size_t normalizedSize,
								  grn_obj *prefix,
								  PGrnNormalizerTableVariantResults *results)
{
	size_t i;
	bool expanded = false;

	if (normalizedSize == 0)
	{
		PGrnNormalizerTableVariantResultsAppend(results,
												GRN_TEXT_VALUE(prefix),
												GRN_TEXT_LEN(prefix));
		return;
	}

	for (i = 0; i < variants->nItems; i++)
	{
		PGrnNormalizerTableVariant *variant = &(variants->items[i]);
		size_t originalPrefixSize;

		if (variant->normalizedSize > normalizedSize)
			continue;
		if (memcmp(normalized, variant->normalized, variant->normalizedSize) != 0)
			continue;

		originalPrefixSize = GRN_TEXT_LEN(prefix);
		GRN_TEXT_PUT(ctx, prefix, variant->target, variant->targetSize);
		PGrnNormalizerTableVariantsExpand(
			variants,
			normalized + variant->normalizedSize,
			normalizedSize - variant->normalizedSize,
			prefix,
			results);
		GRN_BULK_SET_CURR(prefix, GRN_BULK_HEAD(prefix) + originalPrefixSize);
		expanded = true;
	}

	{
		int charLength = grn_charlen(ctx, normalized, normalized + normalizedSize);
		size_t originalPrefixSize;
		if (charLength <= 0)
			return;
		originalPrefixSize = GRN_TEXT_LEN(prefix);
		GRN_TEXT_PUT(ctx, prefix, normalized, charLength);
		PGrnNormalizerTableVariantsExpand(variants,
										  normalized + charLength,
										  normalizedSize - charLength,
										  prefix,
										  results);
		GRN_BULK_SET_CURR(prefix, GRN_BULK_HEAD(prefix) + originalPrefixSize);
	}

	(void) expanded;
}

/**
 * pgroonga_normalizer_table_variants(indexName cstring, normalized text) :
 * text[]
 */
Datum
pgroonga_normalizer_table_variants(PG_FUNCTION_ARGS)
{
	char *indexName = PG_GETARG_CSTRING(0);
	text *normalized = PG_GETARG_TEXT_PP(1);
	Relation index;
	int lexiconIndex;
	grn_obj *lexicon;
	grn_obj normalizers;
	char *normalizedColumnObjectName = NULL;
	size_t normalizedColumnObjectNameSize = 0;
	char *targetColumnName = NULL;
	size_t targetColumnNameSize = 0;
	const char *separator;
	size_t tableNameSize;
	grn_obj *normalizationTable;
	grn_obj *normalizedColumn;
	grn_obj *targetColumn;
	PGrnNormalizerTableVariants variants = {NULL, 0, 0};
	PGrnNormalizerTableVariantResults results = {NULL, NULL, 0, 0};
	grn_obj prefix;
	Datum *datums;
	ArrayType *array;
	size_t i;

	index = PGrnPGResolveIndexName(indexName);
	lexiconIndex = PGrnNormalizerTableResolveLexiconIndex(index);
	lexicon = PGrnLookupLexicon(index, lexiconIndex, ERROR);

	GRN_TEXT_INIT(&normalizers, 0);
	grn_table_get_normalizers_string(ctx, lexicon, &normalizers);
	PGrnCheck("[normalizer-table-variants] failed to get normalizers");
	if (!PGrnNormalizerTableParse(GRN_TEXT_VALUE(&normalizers),
								  GRN_TEXT_LEN(&normalizers),
								  &normalizedColumnObjectName,
								  &normalizedColumnObjectNameSize,
								  &targetColumnName,
								  &targetColumnNameSize))
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("pgroonga: [normalizer-table-variants] "
						"index doesn't use NormalizerTable: <%s>",
						RelationGetRelationName(index))));
	}

	separator = normalizedColumnObjectName + normalizedColumnObjectNameSize;
	while (separator > normalizedColumnObjectName && separator[-1] != '.')
		separator--;
	if (separator == normalizedColumnObjectName)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("pgroonga: [normalizer-table-variants] "
						"invalid NormalizerTable column: <%.*s>",
						(int) normalizedColumnObjectNameSize,
						normalizedColumnObjectName)));
	}

	tableNameSize = (separator - normalizedColumnObjectName) - 1;
	normalizationTable =
		PGrnLookupWithSize(normalizedColumnObjectName, tableNameSize, ERROR);
	normalizedColumn =
		PGrnLookupColumnWithSize(normalizationTable,
								 separator,
								 normalizedColumnObjectNameSize - tableNameSize - 1,
								 ERROR);
	targetColumn = PGrnLookupColumnWithSize(
		normalizationTable, targetColumnName, targetColumnNameSize, ERROR);

	PGrnNormalizerTableLoadVariants(
		&variants, normalizationTable, normalizedColumn, targetColumn);

	GRN_TEXT_INIT(&prefix, 0);
	PGrnNormalizerTableVariantsExpand(&variants,
									  VARDATA_ANY(normalized),
									  VARSIZE_ANY_EXHDR(normalized),
									  &prefix,
									  &results);
	GRN_OBJ_FIN(ctx, &prefix);

	if (results.nItems == 0)
	{
		array = construct_empty_array(TEXTOID);
	}
	else
	{
		datums = palloc(sizeof(Datum) * results.nItems);
		for (i = 0; i < results.nItems; i++)
		{
			datums[i] = PointerGetDatum(
				cstring_to_text_with_len(results.items[i], results.sizes[i]));
		}
		array = construct_array(datums, results.nItems, TEXTOID, -1, false, 'i');
	}

	grn_obj_unref(ctx, targetColumn);
	grn_obj_unref(ctx, normalizedColumn);
	grn_obj_unref(ctx, normalizationTable);
	GRN_OBJ_FIN(ctx, &normalizers);
	grn_obj_unref(ctx, lexicon);
	RelationClose(index);

	PG_RETURN_ARRAYTYPE_P(array);
}
