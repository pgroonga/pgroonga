#include "pgroonga.h"

#include "pgrn-compatible.h"
#include "pgrn-global.h"
#include "pgrn-groonga.h"
#include "pgrn-options.h"
#include "pgrn-value.h"

#ifdef PGRN_SUPPORT_OPTIONS
#	include <access/reloptions.h>
#endif

#ifdef PGRN_SUPPORT_OPTIONS
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
PGrnOptionValidateTokenizer(char *name)
{
	grn_obj *tokenizer_name = &(buffers->tokenizer);
	grn_rc rc;

	if (PGrnIsNoneValue(name))
		return;

	if (strcmp(name, PGRN_DEFAULT_TOKENIZER) == 0)
		return;

	PGrnOptionEnsureLexicon("tokenizer");

	GRN_TEXT_SETS(ctx, tokenizer_name, name);
	rc = grn_obj_set_info(ctx,
						  lexicon,
						  GRN_INFO_DEFAULT_TOKENIZER,
						  tokenizer_name);
	if (rc != GRN_SUCCESS) {
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("pgroonga: invalid tokenizer: <%s>: %s",
						name,
						ctx->errbuf)));
	}
}

static void
PGrnOptionValidateNormalizer(char *name)
{
	grn_obj *normalizer_name = &(buffers->normalizer);
	grn_rc rc;

	if (PGrnIsNoneValue(name))
		return;

	if (strcmp(name, PGRN_DEFAULT_NORMALIZER) == 0)
		return;

	PGrnOptionEnsureLexicon("normalizer");

	GRN_TEXT_SETS(ctx, normalizer_name, name);
	rc = grn_obj_set_info(ctx,
						  lexicon,
						  GRN_INFO_NORMALIZER,
						  normalizer_name);
	if (rc != GRN_SUCCESS) {
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("pgroonga: invalid normalizer: <%s>: %s",
						name,
						ctx->errbuf)));
	}
}

static bool
PGrnIsTokenFilter(grn_obj *object)
{
	if (object->header.type != GRN_PROC)
		return false;

	if (grn_proc_get_type(ctx, object) != GRN_PROC_TOKEN_FILTER)
		return false;

	return true;
}

static void
PGrnOptionValidateTokenFilter(const char *name, size_t nameSize, void *data)
{
	grn_obj *tokenFilter;

	tokenFilter = grn_ctx_get(ctx, name, nameSize);
	if (!tokenFilter)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("pgroonga: nonexistent token filter: <%.*s>",
						(int)nameSize, name)));
	}

	if (!PGrnIsTokenFilter(tokenFilter))
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("pgroonga: not token filter: <%.*s>: %s",
						(int)nameSize, name,
						PGrnInspect(tokenFilter))));
	}
}

static void
PGrnOptionValidateTokenFilters(char *names)
{
	PGrnOptionParseNames(names,
						 PGrnOptionValidateTokenFilter,
						 NULL);
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
PGrnOptionValidatePlugins(char *names)
{
	PGrnOptionParseNames(names,
						 PGrnOptionValidatePlugin,
						 NULL);
}

static void
PGrnOptionValidateLexiconType(char *name)
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
#endif

void
PGrnInitializeOptions(void)
{
	lexicon = NULL;
#ifdef PGRN_SUPPORT_OPTIONS
	PGrnReloptionKind = add_reloption_kind();

	add_string_reloption(PGrnReloptionKind,
						 "tokenizer",
						 "Tokenizer name to be used for full-text search",
						 PGRN_DEFAULT_TOKENIZER,
						 PGrnOptionValidateTokenizer);
	add_string_reloption(PGrnReloptionKind,
						 "normalizer",
						 "Normalizer name to be used as fallback",
						 PGRN_DEFAULT_NORMALIZER,
						 PGrnOptionValidateNormalizer);
	add_string_reloption(PGrnReloptionKind,
						 "token_filters",
						 "Token filter names separated by \",\" "
						 "to be used for full-text search",
						 "",
						 PGrnOptionValidateTokenFilters);
	add_string_reloption(PGrnReloptionKind,
						 "plugins",
						 "Plugin names separated by \",\" to be installed",
						 "",
						 PGrnOptionValidatePlugins);
	add_string_reloption(PGrnReloptionKind,
						 "full_text_search_normalizer",
						 "Normalizer name to be used for full-text search",
						 NULL,
						 PGrnOptionValidateNormalizer);
	add_string_reloption(PGrnReloptionKind,
						 "regexp_search_normalizer",
						 "Normalizer name to be used for regexp search",
						 NULL,
						 PGrnOptionValidateNormalizer);
	add_string_reloption(PGrnReloptionKind,
						 "prefix_search_normalizer",
						 "Normalizer name to be used for prefix search",
						 NULL,
						 PGrnOptionValidateNormalizer);
	add_string_reloption(PGrnReloptionKind,
						 "lexicon_type",
						 "Lexicon type to be used for lexicon",
						 NULL,
						 PGrnOptionValidateLexiconType);
#endif
}

void
PGrnFinalizeOptions(void)
{
	if (lexicon)
	{
		grn_obj_close(ctx, lexicon);
	}
}

#ifdef PGRN_SUPPORT_OPTIONS
static void
PGrnOptionCollectTokenFilter(const char *name,
							 size_t nameSize,
							 void *data)
{
	grn_obj *tokenFilters = data;
	grn_obj *tokenFilter;

	tokenFilter = PGrnLookupWithSize(name, nameSize, ERROR);
	GRN_PTR_PUT(ctx, tokenFilters, tokenFilter);
}
#endif

void
PGrnApplyOptionValues(Relation index,
					  PGrnOptionUseCase useCase,
					  grn_obj **tokenizer,
					  const char *defaultTokenizerName,
					  grn_obj **normalizer,
					  const char *defaultNormalizerName,
					  grn_obj *tokenFilters,
					  grn_table_flags *lexiconType)
{
#ifdef PGRN_SUPPORT_OPTIONS
	PGrnOptions *options;
	const char *tokenizerName;
	const char *normalizerName;
	const char *tokenFilterNames;
	const char *lexiconTypeName;

	*lexiconType = GRN_TABLE_PAT_KEY;

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

		return;
	}

	tokenizerName    = ((const char *) options) + options->tokenizerOffset;
	tokenFilterNames = ((const char *) options) + options->tokenFiltersOffset;

	if (useCase == PGRN_OPTION_USE_CASE_PREFIX_SEARCH)
	{
		*tokenizer = NULL;
	}
	else
	{
		if (PGrnIsExplicitNoneValue(tokenizerName))
		{
			*tokenizer = NULL;
		}
		else if (PGrnIsNoneValue(tokenizerName))
		{
			if (defaultTokenizerName)
				*tokenizer = PGrnLookup(defaultTokenizerName, ERROR);
			else
				*tokenizer = NULL;
		}
		else
		{
			*tokenizer = &(buffers->tokenizer);
			GRN_TEXT_SETS(ctx, *tokenizer, tokenizerName);
		}
	}

	normalizerName = ((const char *) options) + options->normalizerOffset;
	switch (useCase)
	{
	case PGRN_OPTION_USE_CASE_FULL_TEXT_SEARCH:
		if (options->fullTextSearchNormalizerOffset != 0)
		{
			normalizerName =
				((const char *) options) + options->fullTextSearchNormalizerOffset;
		}
		break;
	case PGRN_OPTION_USE_CASE_REGEXP_SEARCH:
		if (options->regexpSearchNormalizerOffset != 0)
		{
			normalizerName =
				((const char *) options) + options->regexpSearchNormalizerOffset;
		}
		break;
	case PGRN_OPTION_USE_CASE_PREFIX_SEARCH:
		if (options->prefixSearchNormalizerOffset != 0)
		{
			normalizerName =
				((const char *) options) + options->prefixSearchNormalizerOffset;
		}
		break;
	default:
		break;
	}

	if (PGrnIsExplicitNoneValue(normalizerName))
	{
		*normalizer = NULL;
	}
	else if (PGrnIsNoneValue(normalizerName))
	{
		if (defaultNormalizerName)
			*normalizer = PGrnLookup(defaultNormalizerName, ERROR);
		else
			*normalizer = NULL;
	}
	else
	{
		*normalizer = &(buffers->normalizer);
		GRN_TEXT_SETS(ctx, *normalizer, normalizerName);
	}

	PGrnOptionParseNames(tokenFilterNames,
						 PGrnOptionCollectTokenFilter,
						 tokenFilters);

	lexiconTypeName = GET_STRING_RELOPTION(options, lexiconTypeOffset);
	if (lexiconTypeName)
	{
		if (strcmp(lexiconTypeName, PGRN_LEXICON_TYPE_HASH_TABLE) == 0)
		{
			*lexiconType = GRN_TABLE_HASH_KEY;
		}
		else if (strcmp(lexiconTypeName, PGRN_LEXICON_TYPE_PATRICIA_TRIE) == 0)
		{
			*lexiconType = GRN_TABLE_PAT_KEY;
		}
		else if (strcmp(lexiconTypeName, PGRN_LEXICON_TYPE_DOUBLE_ARRAY_TRIE) == 0)
		{
			*lexiconType = GRN_TABLE_DAT_KEY;
		}
	}
#endif
}

#ifdef PGRN_SUPPORT_OPTIONS
bytea *
pgroonga_options_raw(Datum reloptions,
					 bool validate)
{
	relopt_value *options;
	PGrnOptions *grnOptions;
	int nOptions;
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
		 offsetof(PGrnOptions, lexiconTypeOffset)}
	};

	options = parseRelOptions(reloptions, validate, PGrnReloptionKind,
							  &nOptions);
	grnOptions = allocateReloptStruct(sizeof(PGrnOptions), options, nOptions);
	fillRelOptions(grnOptions,
				   sizeof(PGrnOptions),
				   options,
				   nOptions,
				   validate,
				   optionsMap,
				   lengthof(optionsMap));
	pfree(options);

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
#endif
