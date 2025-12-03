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
	int modelOffset;
	int nGPULayers;
	int passagePrefixOffset;
	int queryPrefixOffset;
} PGrnOptions;

static relopt_kind PGrnReloptionKind;

static struct PGrnBuffers *buffers = &PGrnBuffers;
static grn_obj *lexicon = NULL;
static grn_language_model_loader *languageModelLoader = NULL;

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

static void
PGrnOptionValidateModel(const char *rawModel)
{
	const char *tag = "[option][model][validate]";
	grn_language_model *model;

	if (!rawModel || rawModel[0] == '\0')
		return;

	if (!PGrnIsLlamaCppAvailable)
		PGrnCheckRC(GRN_FUNCTION_NOT_IMPLEMENTED,
					"%s Groonga's llama.cpp support isn't enabled",
					tag);

	if (!PGrnIsFaissAvailable)
		PGrnCheckRC(GRN_FUNCTION_NOT_IMPLEMENTED,
					"%s Groonga's Faiss support isn't enabled",
					tag);

	grn_language_model_loader_set_model(
		ctx, languageModelLoader, rawModel, strlen(rawModel));
#if GRN_VERSION_OR_LATER(15, 2, 1)
	grn_language_model_loader_set_n_gpu_layers(ctx, languageModelLoader, 0);
#endif
	model = grn_language_model_loader_load(ctx, languageModelLoader);
	if (model)
	{
		grn_language_model_close(ctx, model);
	}
	else
	{
		PGrnCheck("%s failed to load model: <%s>", tag, rawModel);
	}
}

void
PGrnInitializeOptions(void)
{
	const LOCKMODE lock_mode = ShareUpdateExclusiveLock;

	lexicon = NULL;
	languageModelLoader = grn_language_model_loader_open(ctx);
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
	add_string_reloption(PGrnReloptionKind,
						 "model",
						 "Language model name to be used by semantic search",
						 NULL,
						 PGrnOptionValidateModel,
						 lock_mode);
#ifndef GRN_LANGUAGE_MODEL_LOADER_N_GPU_LAYERS_DEFAULT
#	define GRN_LANGUAGE_MODEL_LOADER_N_GPU_LAYERS_DEFAULT 999
#endif
	add_int_reloption(PGrnReloptionKind,
					  "n_gpu_layers",
					  "The number of layers used by language model",
					  GRN_LANGUAGE_MODEL_LOADER_N_GPU_LAYERS_DEFAULT,
					  0,
					  GRN_LANGUAGE_MODEL_LOADER_N_GPU_LAYERS_DEFAULT,
					  lock_mode);
	add_string_reloption(PGrnReloptionKind,
						 "passage_prefix",
						 "Specify prefix for search target text",
						 NULL,
						 false,
						 lock_mode);
	add_string_reloption(PGrnReloptionKind,
						 "query_prefix",
						 "Specify prefix for query text",
						 NULL,
						 false,
						 lock_mode);
}

void
PGrnFinalizeOptions(void)
{
	if (lexicon)
	{
		grn_obj_close(ctx, lexicon);
		lexicon = NULL;
	}
	if (languageModelLoader)
	{
		grn_language_model_loader_close(ctx, languageModelLoader);
		languageModelLoader = NULL;
	}
}

static void
PGrnResolveOptionValuesTokenizer(PGrnOptions *options,
								 Relation index,
								 int i,
								 PGrnOptionUseCase useCase,
								 const char *defaultTokenizer,
								 PGrnResolvedOptions *resolvedOptions)
{
	const char *rawTokenizer;
	TupleDesc desc = RelationGetDescr(index);
	Name name = &(TupleDescAttr(desc, i)->attname);

	if (options->tokenizerMappingOffset != 0 && i >= 0)
	{
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

			resolvedOptions->tokenizer = &(buffers->tokenizer);
			GRN_BULK_REWIND(resolvedOptions->tokenizer);
			PGrnStringSubstituteVariables(value.val.string.val,
										  value.val.string.len,
										  resolvedOptions->tokenizer);
			return;
		}
	}

	rawTokenizer = ((const char *) options) + options->tokenizerOffset;
	if (useCase == PGRN_OPTION_USE_CASE_REGEXP_SEARCH)
	{
		resolvedOptions->tokenizer = PGrnLookup(defaultTokenizer, ERROR);
	}
	else if (useCase == PGRN_OPTION_USE_CASE_PREFIX_SEARCH)
	{
		resolvedOptions->tokenizer = NULL;
	}
	else if (useCase == PGRN_OPTION_USE_CASE_SEMANTIC_SEARCH)
	{
		char codeColumnName[GRN_TABLE_MAX_KEY_SIZE];
		grn_language_model *model;

		PGrnCodeColumnNameEncode(name->data, codeColumnName);

		grn_language_model_loader_set_model(ctx,
											languageModelLoader,
											resolvedOptions->modelName,
											strlen(resolvedOptions->modelName));
#if GRN_VERSION_OR_LATER(15, 2, 1)
		grn_language_model_loader_set_n_gpu_layers(
			ctx, languageModelLoader, resolvedOptions->nGPULayers);
#endif
		model = grn_language_model_loader_load(ctx, languageModelLoader);
		if (!model)
		{
			PGrnCheck("failed to load model: <%s>", resolvedOptions->modelName);
		}
#if GRN_VERSION_OR_LATER(15, 2, 1)
		/* float * 1024 dimensions == 4KiB == The max key size
		 *
		 * If this model uses 1025 or more dimensions, we can't store
		 * a centroid as a lexicon key. In the case, we need to store
		 * a centroid as a column value.
		 */
		resolvedOptions->needCentroidColumn =
			(grn_language_model_get_n_embedding_dimensions(ctx, model) > 1024);
#else
		resolvedOptions->needCentroidColumn = false;
#endif

		resolvedOptions->tokenizer = &(buffers->tokenizer);
		GRN_BULK_REWIND(resolvedOptions->tokenizer);
		grn_text_printf(ctx,
						resolvedOptions->tokenizer,
						"TokenLanguageModelKNN(\"model\", \"%s\", "
						"\"code_column\", \"%s\", "
						"\"n_gpu_layers\", %d",
						resolvedOptions->modelName,
						codeColumnName,
						resolvedOptions->nGPULayers);

		if (resolvedOptions->passagePrefix)
		{
			grn_text_printf(ctx,
							resolvedOptions->tokenizer,
							", \"passage_prefix\", \"%s\"",
							resolvedOptions->passagePrefix);
		}

		if (resolvedOptions->queryPrefix)
		{
			grn_text_printf(ctx,
							resolvedOptions->tokenizer,
							", \"query_prefix\", \"%s\"",
							resolvedOptions->queryPrefix);
		}

		if (resolvedOptions->needCentroidColumn)
		{
			resolvedOptions->lexiconKeyTypeID = GRN_DB_UINT32;
			grn_text_printf(ctx,
							resolvedOptions->tokenizer,
							", \"centroid_column\", \"%s\"",
							PGrnCentroidColumnName);
		}
		else
		{
#if GRN_VERSION_OR_LATER(15, 1, 8)
			resolvedOptions->lexiconKeyTypeID = GRN_DB_SHORT_BINARY;
#else
			resolvedOptions->lexiconKeyTypeID = GRN_DB_SHORT_TEXT;
#endif
		}
		GRN_TEXT_PUTS(ctx, resolvedOptions->tokenizer, ")");
	}
	else
	{
		if (PGrnIsExplicitNoneValue(rawTokenizer))
		{
			resolvedOptions->tokenizer = NULL;
		}
		else if (PGrnIsNoneValue(rawTokenizer))
		{
			if (defaultTokenizer)
				resolvedOptions->tokenizer =
					PGrnLookup(defaultTokenizer, ERROR);
			else
				resolvedOptions->tokenizer = NULL;
		}
		else
		{
			resolvedOptions->tokenizer = &(buffers->tokenizer);
			GRN_TEXT_SETS(ctx, resolvedOptions->tokenizer, rawTokenizer);
		}
	}
}

static void
PGrnResolveOptionValuesNormalizers(PGrnOptions *options,
								   Relation index,
								   int i,
								   PGrnOptionUseCase useCase,
								   const char *defaultNormalizers,
								   PGrnResolvedOptions *resolvedOptions)
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

			resolvedOptions->normalizers = &(buffers->normalizers);
			GRN_BULK_REWIND(resolvedOptions->normalizers);
			PGrnStringSubstituteVariables(value.val.string.val,
										  value.val.string.len,
										  resolvedOptions->normalizers);
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
		resolvedOptions->normalizers = NULL;
	}
	else if (PGrnIsNoneValue(rawNormalizers))
	{
		if (defaultNormalizers)
		{
			resolvedOptions->normalizers = &(buffers->normalizers);
			GRN_BULK_REWIND(resolvedOptions->normalizers);
			PGrnStringSubstituteVariables(defaultNormalizers,
										  strlen(defaultNormalizers),
										  resolvedOptions->normalizers);
		}
		else
		{
			resolvedOptions->normalizers = NULL;
		}
	}
	else
	{
		resolvedOptions->normalizers = &(buffers->normalizers);
		GRN_BULK_REWIND(resolvedOptions->normalizers);
		PGrnStringSubstituteVariables(rawNormalizers,
									  strlen(rawNormalizers),
									  resolvedOptions->normalizers);
	}
}

static void
PGrnResolveOptionValuesIndexFlags(PGrnOptions *options,
								  Relation index,
								  int i,
								  PGrnResolvedOptions *resolvedOptions)
{
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
					resolvedOptions->indexFlags |= GRN_OBJ_INDEX_SMALL;
				else if (GRN_RAW_STRING_EQUAL_CSTRING(rawFlag, "MEDIUM"))
					resolvedOptions->indexFlags |= GRN_OBJ_INDEX_MEDIUM;
				else if (GRN_RAW_STRING_EQUAL_CSTRING(rawFlag, "LARGE"))
					resolvedOptions->indexFlags |= GRN_OBJ_INDEX_LARGE;
				else if (GRN_RAW_STRING_EQUAL_CSTRING(rawFlag, "WITH_WEIGHT"))
					resolvedOptions->indexFlags |= GRN_OBJ_WITH_WEIGHT;
				else if (GRN_RAW_STRING_EQUAL_CSTRING(rawFlag,
													  "WEIGHT_FLOAT32"))
					resolvedOptions->indexFlags |= GRN_OBJ_WEIGHT_FLOAT32;
			}
			if (isTarget)
				return;
		}
	}
}

void
PGrnResolveOptionValues(Relation index,
						int i,
						PGrnOptionUseCase useCase,
						const char *defaultTokenizer,
						const char *defaultNormalizers,
						PGrnResolvedOptions *resolvedOptions)
{
	PGrnOptions *options;
	const char *rawTokenFilters;
	const char *lexiconTypeName;

	options = (PGrnOptions *) (index->rd_options);
	if (!options)
	{
		if (defaultTokenizer)
			resolvedOptions->tokenizer = PGrnLookup(defaultTokenizer, ERROR);
		else
			resolvedOptions->tokenizer = NULL;

		if (defaultNormalizers)
		{
			resolvedOptions->normalizers = &(buffers->normalizers);
			GRN_TEXT_SETS(
				ctx, resolvedOptions->normalizers, defaultNormalizers);
		}
		else
		{
			resolvedOptions->normalizers = NULL;
		}

		resolvedOptions->lexiconType |= GRN_OBJ_TABLE_PAT_KEY;

		return;
	}

	resolvedOptions->modelName = GET_STRING_RELOPTION(options, modelOffset);
	resolvedOptions->nGPULayers = options->nGPULayers;
	resolvedOptions->passagePrefix =
		GET_STRING_RELOPTION(options, passagePrefixOffset);
	resolvedOptions->queryPrefix =
		GET_STRING_RELOPTION(options, queryPrefixOffset);

	PGrnResolveOptionValuesTokenizer(
		options, index, i, useCase, defaultTokenizer, resolvedOptions);

	PGrnResolveOptionValuesNormalizers(
		options, index, i, useCase, defaultNormalizers, resolvedOptions);

	rawTokenFilters = ((const char *) options) + options->tokenFiltersOffset;
	if (!PGrnIsNoneValue(rawTokenFilters))
	{
		resolvedOptions->tokenFilters = &(buffers->tokenFilters);
		GRN_TEXT_SETS(ctx, resolvedOptions->tokenFilters, rawTokenFilters);
	}

	const char *rawPlugins = ((const char *) options) + options->pluginsOffset;
	if (!PGrnIsNoneValue(rawPlugins))
	{
		resolvedOptions->plugins = &(buffers->plugins);
		GRN_BULK_REWIND(resolvedOptions->plugins);
		PGrnOptionParseNames(
			rawPlugins, PGrnOptionAppendName, resolvedOptions->plugins);
	}

	lexiconTypeName = GET_STRING_RELOPTION(options, lexiconTypeOffset);
	if (lexiconTypeName)
	{
		if (strcmp(lexiconTypeName, PGRN_LEXICON_TYPE_HASH_TABLE) == 0)
		{
			resolvedOptions->lexiconType |= GRN_OBJ_TABLE_HASH_KEY;
		}
		else if (strcmp(lexiconTypeName, PGRN_LEXICON_TYPE_PATRICIA_TRIE) == 0)
		{
			resolvedOptions->lexiconType |= GRN_OBJ_TABLE_PAT_KEY;
		}
		else if (strcmp(lexiconTypeName, PGRN_LEXICON_TYPE_DOUBLE_ARRAY_TRIE) ==
				 0)
		{
			resolvedOptions->lexiconType |= GRN_OBJ_TABLE_DAT_KEY;
		}
	}
	else
	{
		if (useCase == PGRN_OPTION_USE_CASE_SEMANTIC_SEARCH)
		{
			resolvedOptions->lexiconType |= GRN_OBJ_TABLE_HASH_KEY;
		}
		else
		{
			resolvedOptions->lexiconType |= GRN_OBJ_TABLE_PAT_KEY;
		}
	}

	PGrnResolveOptionValuesIndexFlags(options, index, i, resolvedOptions);
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
		{"model", RELOPT_TYPE_STRING, offsetof(PGrnOptions, modelOffset)},
		{"n_gpu_layers", RELOPT_TYPE_INT, offsetof(PGrnOptions, nGPULayers)},
		{"passage_prefix",
		 RELOPT_TYPE_STRING,
		 offsetof(PGrnOptions, passagePrefixOffset)},
		{"query_prefix",
		 RELOPT_TYPE_STRING,
		 offsetof(PGrnOptions, queryPrefixOffset)},
	};

	grnOptions = build_reloptions(reloptions,
								  validate,
								  PGrnReloptionKind,
								  sizeof(PGrnOptions),
								  optionsMap,
								  lengthof(optionsMap));

	return (bytea *) grnOptions;
}
