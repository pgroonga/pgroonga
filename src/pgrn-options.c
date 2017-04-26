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
typedef struct PGrnOptions
{
	int32 vl_len_;
	int tokenizerOffset;
	int normalizerOffset;
	int tokenFiltersOffset;
	int pluginsOffset;
} PGrnOptions;

static relopt_kind PGrnReloptionKind;

static grn_ctx *ctx = &PGrnContext;

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

static bool
PGrnIsTokenizer(grn_obj *object)
{
	if (object->header.type != GRN_PROC)
		return false;

	if (grn_proc_get_type(ctx, object) != GRN_PROC_TOKENIZER)
		return false;

	return true;
}

static void
PGrnOptionValidateTokenizer(char *name)
{
	grn_obj *tokenizer;

	if (PGrnIsNoneValue(name))
		return;

	tokenizer = grn_ctx_get(ctx, name, -1);
	if (!tokenizer)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("pgroonga: nonexistent tokenizer: <%s>",
						name)));
	}

	if (!PGrnIsTokenizer(tokenizer))
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("pgroonga: not tokenizer: <%s>: %s",
						name, PGrnInspect(tokenizer))));
	}
}

static bool
PGrnIsNormalizer(grn_obj *object)
{
	if (object->header.type != GRN_PROC)
		return false;

  if (grn_proc_get_type(ctx, object) != GRN_PROC_NORMALIZER)
	  return false;

  return true;
}

static void
PGrnOptionValidateNormalizer(char *name)
{
	grn_obj *normalizer;

	if (PGrnIsNoneValue(name))
		return;

	normalizer = grn_ctx_get(ctx, name, -1);
	if (!normalizer)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("pgroonga: nonexistent normalizer: <%s>",
						name)));
	}

	if (!PGrnIsNormalizer(normalizer))
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("pgroonga: not normalizer: <%s>: %s",
						name, PGrnInspect(normalizer))));
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
#endif

void
PGrnInitializeOptions(void)
{
#ifdef PGRN_SUPPORT_OPTIONS
	PGrnReloptionKind = add_reloption_kind();

	add_string_reloption(PGrnReloptionKind,
						 "tokenizer",
						 "Tokenizer name to be used for full-text search",
						 PGRN_DEFAULT_TOKENIZER,
						 PGrnOptionValidateTokenizer);
	add_string_reloption(PGrnReloptionKind,
						 "normalizer",
						 "Normalizer name to be used for full-text search",
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
#endif
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
					  grn_obj **tokenizer,
					  const char *defaultTokenizerName,
					  grn_obj **normalizer,
					  const char *defaultNormalizerName,
					  grn_obj *tokenFilters)
{
#ifdef PGRN_SUPPORT_OPTIONS
	PGrnOptions *options;
	const char *tokenizerName;
	const char *normalizerName;
	const char *tokenFilterNames;

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
	normalizerName   = ((const char *) options) + options->normalizerOffset;
	tokenFilterNames = ((const char *) options) + options->tokenFiltersOffset;

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
		*tokenizer = PGrnLookup(tokenizerName, ERROR);
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
		*normalizer = PGrnLookup(normalizerName, ERROR);
	}

	PGrnOptionParseNames(tokenFilterNames,
						 PGrnOptionCollectTokenFilter,
						 tokenFilters);
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
		 offsetof(PGrnOptions, pluginsOffset)}
	};

	options = parseRelOptions(reloptions, validate, PGrnReloptionKind,
							  &nOptions);
	grnOptions = allocateReloptStruct(sizeof(PGrnOptions), options, nOptions);
	fillRelOptions(grnOptions, sizeof(PGrnOptions), options, nOptions,
				   validate, optionsMap, lengthof(optionsMap));
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
