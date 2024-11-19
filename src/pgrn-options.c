#include "pgroonga.h"

#include "pgrn-global.h"
#include "pgrn-groonga.h"
#include "pgrn-jsonb.h"
#include "pgrn-options.h"
#include "pgrn-string.h"
#include "pgrn-value.h"

#include <access/reloptions.h>

static const char *PGRN_LEXICON_TYPE_HASH_TABLE = "hash_table";
static const char *PGRN_LEXICON_TYPE_PATRICIA_TRIE = "patricia_trie";
static const char *PGRN_LEXICON_TYPE_DOUBLE_ARRAY_TRIE = "double_array_trie";

typedef struct PGrnOptions
{
	int32 vl_len_;
	int tokenizerOffset;
	int tokenizerMappingOffset;
	int normalizerOffset;
	int tokenFiltersOffset;
	int pluginsOffset;
	int fullTextSearchNormalizerOffset;
	int regexpSearchNormalizerOffset;
	int prefixSearchNormalizerOffset;
	int lexiconTypeOffset;
	bool queryAllowColumn;
	int normalizersOffset;
	int normalizersMappingOffset;
	int indexFlagsMappingOffset;
} PGrnOptions;

static relopt_kind PGrnReloptionKind;

static struct PGrnBuffers *buffers = &PGrnBuffers;
static grn_obj *lexicon = NULL;

typedef void (*PGrnOptionNameFunction)(const char *name,
									   size_t nameSize,
									   void *data);

static void
PGrnOptionAppendName(const char *name, size_t nameSize, void *data)
{
	grn_obj *vector = data;
	grn_vector_add_element(ctx, vector, name, nameSize, 0, GRN_DB_TEXT);
}

static void
PGrnOptionParseNames(const char *names,
					 PGrnOptionNameFunction function,
					 void *data)
{
	const char *start;
	const char *current;

	if (PGrnIsNoneValue(names))
		return;

	for (start = current = names; current[0]; current++)
	{
		switch (current[0])
		{
		case ' ':
			start = current + 1;
			break;
		case ',':
			function(start, current - start, data);
			start = current + 1;
			break;
		default:
			break;
		}
	}

	if (current > start)
	{
		function(start, current - start, data);
	}
}

static void
PGrnOptionEnsureLexicon(const char *context)
{
	if (lexicon)
		grn_obj_close(ctx, lexicon);

	lexicon = grn_table_create(ctx,
							   NULL,
							   0,
							   NULL,
							   GRN_TABLE_PAT_KEY,
							   grn_ctx_at(ctx, GRN_DB_SHORT_TEXT),
							   NULL);
	PGrnCheck("options: %s: failed to create a temporary lexicon", context);
}

static void
PGrnOptionValidateTokenizer(const char *rawTokenizer)
{
	const char *tag = "[option][tokenizer][validate]";
	grn_obj *tokenizer = &(buffers->tokenizer);

	if (PGrnIsNoneValue(rawTokenizer))
		return;

	if (strcmp(rawTokenizer, PGRN_DEFAULT_TOKENIZER) == 0)
		return;

	PGrnOptionEnsureLexicon("tokenizer");

	GRN_TEXT_SETS(ctx, tokenizer, rawTokenizer);
	grn_obj_set_info(ctx, lexicon, GRN_INFO_DEFAULT_TOKENIZER, tokenizer);
	PGrnCheck("%s invalid tokenizer: <%s>", tag, rawTokenizer);
}

static void
PGrnOptionValidateTokenizerMapping(const char *rawTokenizerMapping)
{
	const char *tag = "[option][tokenizer-mapping][validate]";
	grn_obj *tokenizer = &(buffers->tokenizer);
	Jsonb *jsonb;
	JsonbIterator *iter;
	JsonbIteratorToken token;
	JsonbValue value;

	if (PGrnIsNoneValue(rawTokenizerMapping))
		return;

	jsonb = PGrnJSONBParse(rawTokenizerMapping);
	iter = JsonbIteratorInit(&(jsonb->root));

	PGrnOptionEnsureLexicon("tokenizer");

	token = JsonbIteratorNext(&iter, &value, false);
	if (token != WJB_BEGIN_OBJECT)
	{
		PGrnCheckRC(GRN_INVALID_ARGUMENT,
					"%s must be object: %s: <%s>",
					tag,
					PGrnJSONBIteratorTokenToString(token),
					rawTokenizerMapping);
	}

	while (true)
	{
		token = JsonbIteratorNext(&iter, &value, false);
		if (token == WJB_END_OBJECT)
			break;
		if (token != WJB_KEY)
		{
			PGrnCheckRC(GRN_INVALID_ARGUMENT,
						"%s misses key: %s: <%s>",
						tag,
						PGrnJSONBIteratorTokenToString(token),
						rawTokenizerMapping);
		}
		token = JsonbIteratorNext(&iter, &value, false);
		if (token != WJB_VALUE)
		{
			PGrnCheckRC(GRN_INVALID_ARGUMENT,
						"%s misses value: %s: <%s>",
						tag,
						PGrnJSONBIteratorTokenToString(token),
						rawTokenizerMapping);
		}
		if (value.type != jbvString)
		{
			PGrnCheckRC(GRN_INVALID_ARGUMENT,
						"%s value must be string: %s: <%s>",
						tag,
						PGrnJSONBValueTypeToString(value.type),
						rawTokenizerMapping);
		}
		GRN_TEXT_SET(
			ctx, tokenizer, value.val.string.val, value.val.string.len);
		grn_obj_set_info(ctx, lexicon, GRN_INFO_DEFAULT_TOKENIZER, tokenizer);
		PGrnCheck("%s value is invalid tokenizer: <%.*s>",
				  tag,
				  (int) GRN_TEXT_LEN(tokenizer),
				  GRN_TEXT_VALUE(tokenizer));
	}
}

static void
PGrnOptionValidateNormalizers(const char *rawNormalizers)
{
	const char *tag = "[option][normalizers][validate]";
	grn_obj *normalizers = &(buffers->normalizers);

	if (PGrnIsNoneValue(rawNormalizers))
		return;

	if (strcmp(rawNormalizers, PGRN_DEFAULT_NORMALIZERS) == 0)
		return;

	PGrnOptionEnsureLexicon("normalizers");

	GRN_TEXT_SETS(ctx, normalizers, rawNormalizers);
	grn_obj_set_info(ctx, lexicon, GRN_INFO_NORMALIZERS, normalizers);
	PGrnCheck("%s invalid normalizers: <%s>", tag, rawNormalizers);
}

static void
PGrnOptionValidateNormalizersMapping(const char *rawNormalizersMapping)
{
	const char *tag = "[option][normalizers-mapping][validate]";
	grn_obj *normalizers = &(buffers->normalizers);
	Jsonb *jsonb;
	JsonbIterator *iter;
	JsonbIteratorToken token;
	JsonbValue value;

	if (PGrnIsNoneValue(rawNormalizersMapping))
		return;

	jsonb = PGrnJSONBParse(rawNormalizersMapping);
	iter = JsonbIteratorInit(&(jsonb->root));

	PGrnOptionEnsureLexicon("normalizers");

	token = JsonbIteratorNext(&iter, &value, false);
	if (token != WJB_BEGIN_OBJECT)
	{
		PGrnCheckRC(GRN_INVALID_ARGUMENT,
					"%s must be object: %s: <%s>",
					tag,
					PGrnJSONBIteratorTokenToString(token),
					rawNormalizersMapping);
	}

	while (true)
	{
		token = JsonbIteratorNext(&iter, &value, false);
		if (token == WJB_END_OBJECT)
			break;
		if (token != WJB_KEY)
		{
			PGrnCheckRC(GRN_INVALID_ARGUMENT,
						"%s misses key: %s: <%s>",
						tag,
						PGrnJSONBIteratorTokenToString(token),
						rawNormalizersMapping);
		}
		token = JsonbIteratorNext(&iter, &value, false);
		if (token != WJB_VALUE)
		{
			PGrnCheckRC(GRN_INVALID_ARGUMENT,
						"%s misses value: %s: <%s>",
						tag,
						PGrnJSONBIteratorTokenToString(token),
						rawNormalizersMapping);
		}
		if (value.type != jbvString)
		{
			PGrnCheckRC(GRN_INVALID_ARGUMENT,
						"%s value must be string: %s: <%s>",
						tag,
						PGrnJSONBValueTypeToString(value.type),
						rawNormalizersMapping);
		}
		GRN_TEXT_SET(
			ctx, normalizers, value.val.string.val, value.val.string.len);
		grn_obj_set_info(ctx, lexicon, GRN_INFO_NORMALIZERS, normalizers);
		PGrnCheck("%s value is invalid normalizer: <%.*s>",
				  tag,
				  (int) GRN_TEXT_LEN(normalizers),
				  GRN_TEXT_VALUE(normalizers));
	}
}

static void
PGrnOptionValidateTokenFilters(const char *rawTokenFilters)
{
	const char *tag = "[option][token-filters][validate]";
	grn_obj *tokenFilters = &(buffers->tokenFilters);

	if (PGrnIsNoneValue(rawTokenFilters))
		return;

	PGrnOptionEnsureLexicon("token filters");

	GRN_TEXT_SETS(ctx, tokenFilters, rawTokenFilters);
	grn_obj_set_info(ctx, lexicon, GRN_INFO_TOKEN_FILTERS, tokenFilters);
	PGrnCheck("%s invalid token filters: <%s>", tag, rawTokenFilters);
}

static void
PGrnOptionValidatePlugin(const char *name, size_t nameSize, void *data)
{
	const char *tag = "[option][plugin][validate]";
	PGrnRegisterPluginWithSize(name, nameSize, tag);
}

static void
PGrnOptionValidatePlugins(const char *names)
{
	PGrnOptionParseNames(names, PGrnOptionValidatePlugin, NULL);
}

static void
PGrnOptionValidateLexiconType(const char *name)
{
	const char *tag = "[option][lexicon-type][validate]";

	if (!name)
		return;

	if (strcmp(name, PGRN_LEXICON_TYPE_HASH_TABLE) == 0)
		return;

	if (strcmp(name, PGRN_LEXICON_TYPE_PATRICIA_TRIE) == 0)
		return;

	if (strcmp(name, PGRN_LEXICON_TYPE_DOUBLE_ARRAY_TRIE) == 0)
		return;

	PGrnCheckRC(GRN_INVALID_ARGUMENT,
				"%s invalid lexicon type: <%s>: "
				"available types: "
				"[%s, %s, %s]",
				tag,
				name,
				PGRN_LEXICON_TYPE_HASH_TABLE,
				PGRN_LEXICON_TYPE_PATRICIA_TRIE,
				PGRN_LEXICON_TYPE_DOUBLE_ARRAY_TRIE);
}

static void
PGrnOptionValidateIndexFlagsMapping(const char *rawIndexFlagsMapping)
{
	const char *tag = "[option][index-flags-mapping][validate]";
	Jsonb *jsonb;
	JsonbIterator *iter;
	JsonbIteratorToken token;
	JsonbValue value;

	if (PGrnIsNoneValue(rawIndexFlagsMapping))
		return;

	jsonb = PGrnJSONBParse(rawIndexFlagsMapping);
	iter = JsonbIteratorInit(&(jsonb->root));

	token = JsonbIteratorNext(&iter, &value, false);
	if (token != WJB_BEGIN_OBJECT)
	{
		PGrnCheckRC(GRN_INVALID_ARGUMENT,
					"%s must be object: %s: <%s>",
					tag,
					PGrnJSONBIteratorTokenToString(token),
					rawIndexFlagsMapping);
	}

	while (true)
	{
		token = JsonbIteratorNext(&iter, &value, false);
		if (token == WJB_END_OBJECT)
			break;
		if (token != WJB_KEY)
		{
			PGrnCheckRC(GRN_INVALID_ARGUMENT,
						"%s misses key: %s: <%s>",
						tag,
						PGrnJSONBIteratorTokenToString(token),
						rawIndexFlagsMapping);
		}
		token = JsonbIteratorNext(&iter, &value, false);
		if (token != WJB_BEGIN_ARRAY)
		{
			PGrnCheckRC(GRN_INVALID_ARGUMENT,
						"%s value must be array: %s: <%s>",
						tag,
						PGrnJSONBIteratorTokenToString(token),
						rawIndexFlagsMapping);
		}
		while (true)
		{
			grn_raw_string rawFlag;
			token = JsonbIteratorNext(&iter, &value, false);
			if (token == WJB_END_ARRAY)
				break;
			if (value.type != jbvString)
			{
				PGrnCheckRC(GRN_INVALID_ARGUMENT,
							"%s flags must be string: %s: <%s>",
							tag,
							PGrnJSONBValueTypeToString(value.type),
							rawIndexFlagsMapping);
			}
			rawFlag.value = value.val.string.val;
			rawFlag.length = value.val.string.len;
			if (GRN_RAW_STRING_EQUAL_CSTRING(rawFlag, "SMALL") ||
				GRN_RAW_STRING_EQUAL_CSTRING(rawFlag, "MEDIUM") ||
				GRN_RAW_STRING_EQUAL_CSTRING(rawFlag, "LARGE") ||
				GRN_RAW_STRING_EQUAL_CSTRING(rawFlag, "WITH_WEIGHT") ||
				GRN_RAW_STRING_EQUAL_CSTRING(rawFlag, "WEIGHT_FLOAT32"))
			{
				continue;
			}
			PGrnCheckRC(GRN_INVALID_ARGUMENT,
						"%s flags have invalid flag: <%.*s>: %s",
						tag,
						(int) (rawFlag.length),
						rawFlag.value,
						ctx->errbuf);
		}
	}
}

void
PGrnInitializeOptions(void)
{
	const LOCKMODE lock_mode = ShareUpdateExclusiveLock;

	lexicon = NULL;
	PGrnReloptionKind = add_reloption_kind();

	add_string_reloption(PGrnReloptionKind,
						 "tokenizer",
						 "Tokenizer name to be used for full-text search",
						 PGRN_DEFAULT_TOKENIZER,
						 PGrnOptionValidateTokenizer,
						 lock_mode);
	add_string_reloption(PGrnReloptionKind,
						 "tokenizer_mapping",
						 "Mapping to specify tokenizer to be used "
						 "for each target",
						 NULL,
						 PGrnOptionValidateTokenizerMapping,
						 lock_mode);
	add_string_reloption(PGrnReloptionKind,
						 "normalizer",
						 "Normalizers to be used as fallback. "
						 "This is deprecated since 2.3.1. "
						 "Use normalizers instead",
						 NULL,
						 PGrnOptionValidateNormalizers,
						 lock_mode);
	add_string_reloption(PGrnReloptionKind,
						 "token_filters",
						 "Token filter names separated by \",\" "
						 "to be used for full-text search",
						 "",
						 PGrnOptionValidateTokenFilters,
						 lock_mode);
	add_string_reloption(PGrnReloptionKind,
						 "plugins",
						 "Plugin names separated by \",\" to be installed",
						 "",
						 PGrnOptionValidatePlugins,
						 lock_mode);
	add_string_reloption(PGrnReloptionKind,
						 "full_text_search_normalizer",
						 "Normalizers to be used for full-text search. "
						 "This is deprecated since 2.3.1. "
						 "Use normalizers_mapping instead",
						 NULL,
						 PGrnOptionValidateNormalizers,
						 lock_mode);
	add_string_reloption(PGrnReloptionKind,
						 "regexp_search_normalizer",
						 "Normalizers to be used for regexp search. "
						 "This is deprecated since 2.3.1. "
						 "Use normalizers_mapping instead",
						 NULL,
						 PGrnOptionValidateNormalizers,
						 lock_mode);
	add_string_reloption(PGrnReloptionKind,
						 "prefix_search_normalizer",
						 "Normalizers to be used for prefix search"
						 "This is deprecated since 2.3.1. "
						 "Use normalizers_mapping instead",
						 NULL,
						 PGrnOptionValidateNormalizers,
						 lock_mode);
	add_string_reloption(PGrnReloptionKind,
						 "lexicon_type",
						 "Lexicon type to be used for lexicon",
						 NULL,
						 PGrnOptionValidateLexiconType,
						 lock_mode);
	add_bool_reloption(PGrnReloptionKind,
					   "query_allow_column",
					   "Accept column:... syntax in query",
					   false,
					   lock_mode);
	add_string_reloption(PGrnReloptionKind,
						 "normalizers",
						 "Normalizers to be used as fallback",
						 NULL,
						 PGrnOptionValidateNormalizers,
						 lock_mode);
	add_string_reloption(PGrnReloptionKind,
						 "normalizers_mapping",
						 "Mapping to specify normalizers to be used "
						 "for each target",
						 NULL,
						 PGrnOptionValidateNormalizersMapping,
						 lock_mode);
	add_string_reloption(PGrnReloptionKind,
						 "index_flags_mapping",
						 "Mapping to specify index flags to be used "
						 "for each target",
						 NULL,
						 PGrnOptionValidateIndexFlagsMapping,
						 lock_mode);
}

void
PGrnFinalizeOptions(void)
{
	if (lexicon)
	{
		grn_obj_close(ctx, lexicon);
	}
}

static void
PGrnApplyOptionValuesTokenizer(PGrnOptions *options,
							   Relation index,
							   int i,
							   PGrnOptionUseCase useCase,
							   grn_obj **tokenizer,
							   const char *defaultTokenizer)
{
	const char *rawTokenizer;

	if (options->tokenizerMappingOffset != 0 && i >= 0)
	{
		TupleDesc desc = RelationGetDescr(index);
		Name name = &(TupleDescAttr(desc, i)->attname);
		const char *rawTokenizerMapping =
			((const char *) options) + options->tokenizerMappingOffset;
		Jsonb *jsonb = PGrnJSONBParse(rawTokenizerMapping);
		JsonbIterator *iter;
		JsonbValue value;

		iter = JsonbIteratorInit(&(jsonb->root));
		/* This JSON is validated. */
		/* WJB_BEGIN_OBJECT */
		JsonbIteratorNext(&iter, &value, false);
		while (true)
		{
			bool isTarget;

			/* WJB_KEY */
			if (JsonbIteratorNext(&iter, &value, false) == WJB_END_OBJECT)
				break;
			isTarget = (value.val.string.len == strlen(name->data) &&
						memcmp(value.val.string.val,
							   name->data,
							   value.val.string.len) == 0);
			/* WJB_VALUE */
			JsonbIteratorNext(&iter, &value, false);
			if (!isTarget)
				continue;

			*tokenizer = &(buffers->tokenizer);
			GRN_BULK_REWIND(*tokenizer);
			PGrnStringSubstituteVariables(
				value.val.string.val, value.val.string.len, *tokenizer);
			return;
		}
	}

	rawTokenizer = ((const char *) options) + options->tokenizerOffset;
	if (useCase == PGRN_OPTION_USE_CASE_REGEXP_SEARCH)
	{
		*tokenizer = PGrnLookup(defaultTokenizer, ERROR);
	}
	else if (useCase == PGRN_OPTION_USE_CASE_PREFIX_SEARCH)
	{
		*tokenizer = NULL;
	}
	else
	{
		if (PGrnIsExplicitNoneValue(rawTokenizer))
		{
			*tokenizer = NULL;
		}
		else if (PGrnIsNoneValue(rawTokenizer))
		{
			if (defaultTokenizer)
				*tokenizer = PGrnLookup(defaultTokenizer, ERROR);
			else
				*tokenizer = NULL;
		}
		else
		{
			*tokenizer = &(buffers->tokenizer);
			GRN_TEXT_SETS(ctx, *tokenizer, rawTokenizer);
		}
	}
}

static void
PGrnApplyOptionValuesNormalizers(PGrnOptions *options,
								 Relation index,
								 int i,
								 PGrnOptionUseCase useCase,
								 grn_obj **normalizers,
								 const char *defaultNormalizers)
{
	const char *rawNormalizers;

	if (options->normalizersMappingOffset != 0 && i >= 0)
	{
		TupleDesc desc = RelationGetDescr(index);
		Name name = &(TupleDescAttr(desc, i)->attname);
		const char *rawNormalizersMapping =
			((const char *) options) + options->normalizersMappingOffset;
		Jsonb *jsonb = PGrnJSONBParse(rawNormalizersMapping);
		JsonbIterator *iter;
		JsonbValue value;

		iter = JsonbIteratorInit(&(jsonb->root));
		/* This JSON is validated. */
		/* WJB_BEGIN_OBJECT */
		JsonbIteratorNext(&iter, &value, false);
		while (true)
		{
			bool isTarget;

			/* WJB_KEY */
			if (JsonbIteratorNext(&iter, &value, false) == WJB_END_OBJECT)
				break;
			isTarget = (value.val.string.len == strlen(name->data) &&
						memcmp(value.val.string.val,
							   name->data,
							   value.val.string.len) == 0);
			/* WJB_VALUE */
			JsonbIteratorNext(&iter, &value, false);
			if (!isTarget)
				continue;

			*normalizers = &(buffers->normalizers);
			GRN_BULK_REWIND(*normalizers);
			PGrnStringSubstituteVariables(
				value.val.string.val, value.val.string.len, *normalizers);
			return;
		}
	}

	{
		int normalizersOffset = options->normalizersOffset;
		/* For backward compatibility. */
		if (normalizersOffset == 0)
		{
			normalizersOffset = options->normalizerOffset;
		}
		if (normalizersOffset == 0)
		{
			rawNormalizers = NULL;
		}
		else
		{
			rawNormalizers = ((const char *) options) + normalizersOffset;
		}
	}
	switch (useCase)
	{
	case PGRN_OPTION_USE_CASE_FULL_TEXT_SEARCH:
		if (options->fullTextSearchNormalizerOffset != 0)
		{
			rawNormalizers = ((const char *) options) +
							 options->fullTextSearchNormalizerOffset;
		}
		break;
	case PGRN_OPTION_USE_CASE_REGEXP_SEARCH:
		if (options->regexpSearchNormalizerOffset != 0)
		{
			rawNormalizers = ((const char *) options) +
							 options->regexpSearchNormalizerOffset;
		}
		break;
	case PGRN_OPTION_USE_CASE_PREFIX_SEARCH:
		if (options->prefixSearchNormalizerOffset != 0)
		{
			rawNormalizers = ((const char *) options) +
							 options->prefixSearchNormalizerOffset;
		}
		break;
	default:
		break;
	}

	if (PGrnIsExplicitNoneValue(rawNormalizers))
	{
		*normalizers = NULL;
	}
	else if (PGrnIsNoneValue(rawNormalizers))
	{
		if (defaultNormalizers)
		{
			*normalizers = &(buffers->normalizers);
			GRN_BULK_REWIND(*normalizers);
			PGrnStringSubstituteVariables(
				defaultNormalizers, strlen(defaultNormalizers), *normalizers);
		}
		else
		{
			*normalizers = NULL;
		}
	}
	else
	{
		*normalizers = &(buffers->normalizers);
		GRN_BULK_REWIND(*normalizers);
		PGrnStringSubstituteVariables(
			rawNormalizers, strlen(rawNormalizers), *normalizers);
	}
}

static void
PGrnApplyOptionValuesIndexFlags(PGrnOptions *options,
								Relation index,
								int i,
								grn_column_flags *indexFlags)
{
	if (!indexFlags)
		return;

	if (options->indexFlagsMappingOffset != 0 && i >= 0)
	{
		TupleDesc desc = RelationGetDescr(index);
		Name name = &(TupleDescAttr(desc, i)->attname);
		const char *rawIndexFlagsMapping =
			((const char *) options) + options->indexFlagsMappingOffset;
		Jsonb *jsonb = PGrnJSONBParse(rawIndexFlagsMapping);
		JsonbIterator *iter;
		JsonbValue value;

		iter = JsonbIteratorInit(&(jsonb->root));
		/* This JSON is validated. */
		/* WJB_BEGIN_OBJECT */
		JsonbIteratorNext(&iter, &value, false);
		while (true)
		{
			bool isTarget;

			/* WJB_KEY */
			if (JsonbIteratorNext(&iter, &value, false) == WJB_END_OBJECT)
				break;
			isTarget = (value.val.string.len == strlen(name->data) &&
						memcmp(value.val.string.val,
							   name->data,
							   value.val.string.len) == 0);
			/* WJB_BEGIN_ARRAY */
			JsonbIteratorNext(&iter, &value, false);
			while (JsonbIteratorNext(&iter, &value, false) != WJB_END_ARRAY)
			{
				grn_raw_string rawFlag;

				if (!isTarget)
					continue;

				/* WJB_VALUE/jbvString */
				rawFlag.value = value.val.string.val;
				rawFlag.length = value.val.string.len;
				if (GRN_RAW_STRING_EQUAL_CSTRING(rawFlag, "SMALL"))
					*indexFlags |= GRN_OBJ_INDEX_SMALL;
				else if (GRN_RAW_STRING_EQUAL_CSTRING(rawFlag, "MEDIUM"))
					*indexFlags |= GRN_OBJ_INDEX_MEDIUM;
				else if (GRN_RAW_STRING_EQUAL_CSTRING(rawFlag, "LARGE"))
					*indexFlags |= GRN_OBJ_INDEX_LARGE;
				else if (GRN_RAW_STRING_EQUAL_CSTRING(rawFlag, "WITH_WEIGHT"))
					*indexFlags |= GRN_OBJ_WITH_WEIGHT;
				else if (GRN_RAW_STRING_EQUAL_CSTRING(rawFlag,
													  "WEIGHT_FLOAT32"))
					*indexFlags |= GRN_OBJ_WEIGHT_FLOAT32;
			}
			if (isTarget)
				return;
		}
	}
}

void
PGrnApplyOptionValues(Relation index,
					  int i,
					  PGrnOptionUseCase useCase,
					  grn_obj **tokenizer,
					  const char *defaultTokenizer,
					  grn_obj **normalizers,
					  const char *defaultNormalizers,
					  grn_obj **tokenFilters,
					  grn_obj **plugins,
					  grn_table_flags *lexiconType,
					  grn_column_flags *indexFlags)
{
	PGrnOptions *options;
	const char *rawTokenFilters;
	const char *lexiconTypeName;

	options = (PGrnOptions *) (index->rd_options);
	if (!options)
	{
		if (defaultTokenizer)
			*tokenizer = PGrnLookup(defaultTokenizer, ERROR);
		else
			*tokenizer = NULL;

		if (defaultNormalizers)
		{
			*normalizers = &(buffers->normalizers);
			GRN_TEXT_SETS(ctx, *normalizers, defaultNormalizers);
		}
		else
		{
			*normalizers = NULL;
		}

		*lexiconType |= GRN_OBJ_TABLE_PAT_KEY;

		return;
	}

	PGrnApplyOptionValuesTokenizer(
		options, index, i, useCase, tokenizer, defaultTokenizer);

	PGrnApplyOptionValuesNormalizers(
		options, index, i, useCase, normalizers, defaultNormalizers);

	rawTokenFilters = ((const char *) options) + options->tokenFiltersOffset;
	if (!PGrnIsNoneValue(rawTokenFilters))
	{
		*tokenFilters = &(buffers->tokenFilters);
		GRN_TEXT_SETS(ctx, *tokenFilters, rawTokenFilters);
	}

	if (plugins)
	{
		const char *rawPlugins =
			((const char *) options) + options->pluginsOffset;
		if (!PGrnIsNoneValue(rawPlugins))
		{
			*plugins = &(buffers->plugins);
			GRN_BULK_REWIND(*plugins);
			PGrnOptionParseNames(rawPlugins, PGrnOptionAppendName, *plugins);
		}
	}

	lexiconTypeName = GET_STRING_RELOPTION(options, lexiconTypeOffset);
	if (lexiconTypeName)
	{
		if (strcmp(lexiconTypeName, PGRN_LEXICON_TYPE_HASH_TABLE) == 0)
		{
			*lexiconType |= GRN_OBJ_TABLE_HASH_KEY;
		}
		else if (strcmp(lexiconTypeName, PGRN_LEXICON_TYPE_PATRICIA_TRIE) == 0)
		{
			*lexiconType |= GRN_OBJ_TABLE_PAT_KEY;
		}
		else if (strcmp(lexiconTypeName, PGRN_LEXICON_TYPE_DOUBLE_ARRAY_TRIE) ==
				 0)
		{
			*lexiconType |= GRN_OBJ_TABLE_DAT_KEY;
		}
	}
	else
	{
		*lexiconType |= GRN_OBJ_TABLE_PAT_KEY;
	}

	PGrnApplyOptionValuesIndexFlags(options, index, i, indexFlags);
}

grn_expr_flags
PGrnOptionsGetExprParseFlags(Relation index)
{
	grn_expr_flags flags = 0;
	PGrnOptions *options;

	options = (PGrnOptions *) (index->rd_options);
	if (!options)
		return flags;

	if (options->queryAllowColumn)
		flags |= GRN_EXPR_ALLOW_COLUMN;

	return flags;
}

bytea *
pgroonga_options(Datum reloptions, bool validate)
{
	PGrnOptions *grnOptions;
	const relopt_parse_elt optionsMap[] = {
		{"tokenizer",
		 RELOPT_TYPE_STRING,
		 offsetof(PGrnOptions, tokenizerOffset)},
		{"tokenizer_mapping",
		 RELOPT_TYPE_STRING,
		 offsetof(PGrnOptions, tokenizerMappingOffset)},
		{"normalizer",
		 RELOPT_TYPE_STRING,
		 offsetof(PGrnOptions, normalizerOffset)},
		{"token_filters",
		 RELOPT_TYPE_STRING,
		 offsetof(PGrnOptions, tokenFiltersOffset)},
		{"plugins", RELOPT_TYPE_STRING, offsetof(PGrnOptions, pluginsOffset)},
		{"full_text_search_normalizer",
		 RELOPT_TYPE_STRING,
		 offsetof(PGrnOptions, fullTextSearchNormalizerOffset)},
		{"regexp_search_normalizer",
		 RELOPT_TYPE_STRING,
		 offsetof(PGrnOptions, regexpSearchNormalizerOffset)},
		{"prefix_search_normalizer",
		 RELOPT_TYPE_STRING,
		 offsetof(PGrnOptions, prefixSearchNormalizerOffset)},
		{"lexicon_type",
		 RELOPT_TYPE_STRING,
		 offsetof(PGrnOptions, lexiconTypeOffset)},
		{"query_allow_column",
		 RELOPT_TYPE_BOOL,
		 offsetof(PGrnOptions, queryAllowColumn)},
		{"normalizers",
		 RELOPT_TYPE_STRING,
		 offsetof(PGrnOptions, normalizersOffset)},
		{"normalizers_mapping",
		 RELOPT_TYPE_STRING,
		 offsetof(PGrnOptions, normalizersMappingOffset)},
		{"index_flags_mapping",
		 RELOPT_TYPE_STRING,
		 offsetof(PGrnOptions, indexFlagsMappingOffset)},
	};

#ifdef PGRN_HAVE_BUILD_RELOPTIONS
	grnOptions = build_reloptions(reloptions,
								  validate,
								  PGrnReloptionKind,
								  sizeof(PGrnOptions),
								  optionsMap,
								  lengthof(optionsMap));
#else
	{
		relopt_value *options;
		int nOptions;
		options =
			parseRelOptions(reloptions, validate, PGrnReloptionKind, &nOptions);
		grnOptions =
			allocateReloptStruct(sizeof(PGrnOptions), options, nOptions);
		fillRelOptions(grnOptions,
					   sizeof(PGrnOptions),
					   options,
					   nOptions,
					   validate,
					   optionsMap,
					   lengthof(optionsMap));
		pfree(options);
	}
#endif

	return (bytea *) grnOptions;
}
