#include "pgroonga.h"

#include "pgrn-compatible.h"
#include "pgrn-global.h"
#include "pgrn-groonga.h"
#include "pgrn-options.h"
#include "pgrn-value.h"

#include <access/reloptions.h>

static const char *PGRN_LEXICON_TYPE_HASH_TABLE = "hash_table";
static const char *PGRN_LEXICON_TYPE_PATRICIA_TRIE = "patricia_trie";
static const char *PGRN_LEXICON_TYPE_DOUBLE_ARRAY_TRIE = "double_array_trie";

typedef struct PGrnOptions
{
	int32 vl_len_;
	int tokenizerOffset;
	int normalizerOffset;
	int tokenFiltersOffset;
	int pluginsOffset;
	int fullTextSearchNormalizerOffset;
	int regexpSearchNormalizerOffset;
	int prefixSearchNormalizerOffset;
	int lexiconTypeOffset;
	bool queryAllowColumn;
} PGrnOptions;

static relopt_kind PGrnReloptionKind;

static grn_ctx *ctx = &PGrnContext;
static struct PGrnBuffers *buffers = &PGrnBuffers;
static grn_obj *lexicon = NULL;

PGRN_FUNCTION_INFO_V1(pgroonga_options);

typedef void (*PGrnOptionNameFunction)(const char *name,
									   size_t nameSize,
									   void *data);

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

	if (current > start) {
		function(start, current - start, data);
	}
}

static void
PGrnOptionEnsureLexicon(const char *context)
{
	if (lexicon)
		grn_obj_close(ctx, lexicon);

	lexicon = grn_table_create(ctx,
							   NULL, 0,
							   NULL,
							   GRN_TABLE_PAT_KEY,
							   grn_ctx_at(ctx, GRN_DB_SHORT_TEXT),
							   NULL);
	PGrnCheck("options: %s: failed to create a temporary lexicon",
			  context);
}

static void
PGrnOptionValidateTokenizer(PGrnStringOptionValue rawTokenizer)
{
	grn_obj *tokenizer = &(buffers->tokenizer);
	grn_rc rc;

	if (PGrnIsNoneValue(rawTokenizer))
		return;

	if (strcmp(rawTokenizer, PGRN_DEFAULT_TOKENIZER) == 0)
		return;

	PGrnOptionEnsureLexicon("tokenizer");

	GRN_TEXT_SETS(ctx, tokenizer, rawTokenizer);
	rc = grn_obj_set_info(ctx,
						  lexicon,
						  GRN_INFO_DEFAULT_TOKENIZER,
						  tokenizer);
	if (rc != GRN_SUCCESS) {
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("pgroonga: invalid tokenizer: <%s>: %s",
						rawTokenizer,
						ctx->errbuf)));
	}
}

static void
PGrnOptionValidateNormalizer(PGrnStringOptionValue rawNormalizer)
{
	grn_obj *normalizer = &(buffers->normalizer);
	grn_rc rc;

	if (PGrnIsNoneValue(rawNormalizer))
		return;

	if (strcmp(rawNormalizer, PGRN_DEFAULT_NORMALIZER) == 0)
		return;

	PGrnOptionEnsureLexicon("normalizer");

	GRN_TEXT_SETS(ctx, normalizer, rawNormalizer);
	rc = grn_obj_set_info(ctx,
						  lexicon,
						  GRN_INFO_NORMALIZER,
						  normalizer);
	if (rc != GRN_SUCCESS) {
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("pgroonga: invalid normalizer: <%s>: %s",
						rawNormalizer,
						ctx->errbuf)));
	}
}

static void
PGrnOptionValidateTokenFilters(PGrnStringOptionValue rawTokenFilters)
{
	grn_obj *tokenFilters = &(buffers->tokenFilters);
	grn_rc rc;

	if (PGrnIsNoneValue(rawTokenFilters))
		return;

	PGrnOptionEnsureLexicon("token filters");

	GRN_TEXT_SETS(ctx, tokenFilters, rawTokenFilters);
	rc = grn_obj_set_info(ctx,
						  lexicon,
						  GRN_INFO_TOKEN_FILTERS,
						  tokenFilters);
	if (rc != GRN_SUCCESS) {
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("pgroonga: invalid token filters: <%s>: %s",
						rawTokenFilters,
						ctx->errbuf)));
	}
}

static void
PGrnOptionValidatePlugin(const char *name,
						 size_t nameSize,
						 void *data)
{
	char pluginName[MAXPGPATH];

	grn_strncpy(pluginName, MAXPGPATH, name, nameSize);
	pluginName[nameSize] = '\0';
	grn_plugin_register(ctx, pluginName);
	if (ctx->rc != GRN_SUCCESS)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("pgroonga: failed to register plugin: <%.*s>: %s",
						(int)nameSize, name,
						ctx->errbuf)));
	}
}

static void
PGrnOptionValidatePlugins(PGrnStringOptionValue names)
{
	PGrnOptionParseNames(names,
						 PGrnOptionValidatePlugin,
						 NULL);
}

static void
PGrnOptionValidateLexiconType(PGrnStringOptionValue name)
{
	if (!name)
		return;

	if (strcmp(name, PGRN_LEXICON_TYPE_HASH_TABLE) == 0)
		return;

	if (strcmp(name, PGRN_LEXICON_TYPE_PATRICIA_TRIE) == 0)
		return;

	if (strcmp(name, PGRN_LEXICON_TYPE_DOUBLE_ARRAY_TRIE) == 0)
		return;

	ereport(ERROR,
			(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			 errmsg("pgroonga: invalid lexicon type: <%s>: "
					"available types: "
					"[%s, %s, %s]",
					name,
					PGRN_LEXICON_TYPE_HASH_TABLE,
					PGRN_LEXICON_TYPE_PATRICIA_TRIE,
					PGRN_LEXICON_TYPE_DOUBLE_ARRAY_TRIE)));
}

void
PGrnInitializeOptions(void)
{
#ifdef PGRN_SUPPORT_OPTION_LOCK_MODE
	const LOCKMODE lock_mode = ShareUpdateExclusiveLock;
#endif

	lexicon = NULL;
	PGrnReloptionKind = add_reloption_kind();

	pgrn_add_string_reloption(PGrnReloptionKind,
							  "tokenizer",
							  "Tokenizer name to be used for full-text search",
							  PGRN_DEFAULT_TOKENIZER,
							  PGrnOptionValidateTokenizer,
							  lock_mode);
	pgrn_add_string_reloption(PGrnReloptionKind,
							  "normalizer",
							  "Normalizer name to be used as fallback",
							  PGRN_DEFAULT_NORMALIZER,
							  PGrnOptionValidateNormalizer,
							  lock_mode);
	pgrn_add_string_reloption(PGrnReloptionKind,
							  "token_filters",
							  "Token filter names separated by \",\" "
							  "to be used for full-text search",
							  "",
							  PGrnOptionValidateTokenFilters,
							  lock_mode);
	pgrn_add_string_reloption(PGrnReloptionKind,
							  "plugins",
							  "Plugin names separated by \",\" to be installed",
							  "",
							  PGrnOptionValidatePlugins,
							  lock_mode);
	pgrn_add_string_reloption(PGrnReloptionKind,
							  "full_text_search_normalizer",
							  "Normalizer name to be used for full-text search",
							  NULL,
							  PGrnOptionValidateNormalizer,
							  lock_mode);
	pgrn_add_string_reloption(PGrnReloptionKind,
							  "regexp_search_normalizer",
							  "Normalizer name to be used for regexp search",
							  NULL,
							  PGrnOptionValidateNormalizer,
							  lock_mode);
	pgrn_add_string_reloption(PGrnReloptionKind,
							  "prefix_search_normalizer",
							  "Normalizer name to be used for prefix search",
							  NULL,
							  PGrnOptionValidateNormalizer,
							  lock_mode);
	pgrn_add_string_reloption(PGrnReloptionKind,
							  "lexicon_type",
							  "Lexicon type to be used for lexicon",
							  NULL,
							  PGrnOptionValidateLexiconType,
							  lock_mode);
	pgrn_add_bool_reloption(PGrnReloptionKind,
							"query_allow_column",
							"Accept column:... syntax in query",
							false,
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

void
PGrnApplyOptionValues(Relation index,
					  PGrnOptionUseCase useCase,
					  grn_obj **tokenizer,
					  const char *defaultTokenizerName,
					  grn_obj **normalizer,
					  const char *defaultNormalizerName,
					  grn_obj **tokenFilters,
					  grn_table_flags *lexiconType)
{
	PGrnOptions *options;
	const char *rawTokenizer;
	const char *rawNormalizer;
	const char *rawTokenFilters;
	const char *lexiconTypeName;

	options = (PGrnOptions *) (index->rd_options);
	if (!options)
	{
		if (defaultTokenizerName)
			*tokenizer = PGrnLookup(defaultTokenizerName, ERROR);
		else
			*tokenizer = NULL;

		if (defaultNormalizerName)
			*normalizer = PGrnLookup(defaultNormalizerName, ERROR);
		else
			*normalizer = NULL;

		*lexiconType |= GRN_OBJ_TABLE_PAT_KEY;

		return;
	}

	rawTokenizer = ((const char *) options) + options->tokenizerOffset;

	if (useCase == PGRN_OPTION_USE_CASE_PREFIX_SEARCH)
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
			if (defaultTokenizerName)
				*tokenizer = PGrnLookup(defaultTokenizerName, ERROR);
			else
				*tokenizer = NULL;
		}
		else
		{
			*tokenizer = &(buffers->tokenizer);
			GRN_TEXT_SETS(ctx, *tokenizer, rawTokenizer);
		}
	}

	rawNormalizer = ((const char *) options) + options->normalizerOffset;
	switch (useCase)
	{
	case PGRN_OPTION_USE_CASE_FULL_TEXT_SEARCH:
		if (options->fullTextSearchNormalizerOffset != 0)
		{
			rawNormalizer =
				((const char *) options) + options->fullTextSearchNormalizerOffset;
		}
		break;
	case PGRN_OPTION_USE_CASE_REGEXP_SEARCH:
		if (options->regexpSearchNormalizerOffset != 0)
		{
			rawNormalizer =
				((const char *) options) + options->regexpSearchNormalizerOffset;
		}
		break;
	case PGRN_OPTION_USE_CASE_PREFIX_SEARCH:
		if (options->prefixSearchNormalizerOffset != 0)
		{
			rawNormalizer =
				((const char *) options) + options->prefixSearchNormalizerOffset;
		}
		break;
	default:
		break;
	}

	if (PGrnIsExplicitNoneValue(rawNormalizer))
	{
		*normalizer = NULL;
	}
	else if (PGrnIsNoneValue(rawNormalizer))
	{
		if (defaultNormalizerName)
			*normalizer = PGrnLookup(defaultNormalizerName, ERROR);
		else
			*normalizer = NULL;
	}
	else
	{
		*normalizer = &(buffers->normalizer);
		GRN_TEXT_SETS(ctx, *normalizer, rawNormalizer);
	}

	rawTokenFilters = ((const char *) options) + options->tokenFiltersOffset;
	if (!PGrnIsNoneValue(rawTokenFilters))
	{
		*tokenFilters = &(buffers->tokenFilters);
		GRN_TEXT_SETS(ctx, *tokenFilters, rawTokenFilters);
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
		else if (strcmp(lexiconTypeName, PGRN_LEXICON_TYPE_DOUBLE_ARRAY_TRIE) == 0)
		{
			*lexiconType |= GRN_OBJ_TABLE_DAT_KEY;
		}
	}
	else
	{
		*lexiconType |= GRN_OBJ_TABLE_PAT_KEY;
	}
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
pgroonga_options_raw(Datum reloptions,
					 bool validate)
{
	PGrnOptions *grnOptions;
	const relopt_parse_elt optionsMap[] = {
		{"tokenizer", RELOPT_TYPE_STRING,
		 offsetof(PGrnOptions, tokenizerOffset)},
		{"normalizer", RELOPT_TYPE_STRING,
		 offsetof(PGrnOptions, normalizerOffset)},
		{"token_filters", RELOPT_TYPE_STRING,
		 offsetof(PGrnOptions, tokenFiltersOffset)},
		{"plugins", RELOPT_TYPE_STRING,
		 offsetof(PGrnOptions, pluginsOffset)},
		{"full_text_search_normalizer", RELOPT_TYPE_STRING,
		 offsetof(PGrnOptions, fullTextSearchNormalizerOffset)},
		{"regexp_search_normalizer", RELOPT_TYPE_STRING,
		 offsetof(PGrnOptions, regexpSearchNormalizerOffset)},
		{"prefix_search_normalizer", RELOPT_TYPE_STRING,
		 offsetof(PGrnOptions, prefixSearchNormalizerOffset)},
		{"lexicon_type", RELOPT_TYPE_STRING,
		 offsetof(PGrnOptions, lexiconTypeOffset)},
		{"query_allow_column", RELOPT_TYPE_BOOL,
		 offsetof(PGrnOptions, queryAllowColumn)},
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
		options = parseRelOptions(reloptions,
								  validate,
								  PGrnReloptionKind,
								  &nOptions);
		grnOptions = allocateReloptStruct(sizeof(PGrnOptions),
										  options,
										  nOptions);
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

/**
 * pgroonga.options() -- amoptions
 */
Datum
pgroonga_options(PG_FUNCTION_ARGS)
{
	Datum reloptions = PG_GETARG_DATUM(0);
	bool validate = PG_GETARG_BOOL(1);
	bytea *grnOptions;

	grnOptions = pgroonga_options_raw(reloptions, validate);

	PG_RETURN_BYTEA_P(grnOptions);
}
