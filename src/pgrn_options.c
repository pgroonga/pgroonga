#include "pgroonga.h"

#include "pgrn_compatible.h"
#include "pgrn_global.h"
#include "pgrn_inspect.h"
#include "pgrn_options.h"
#include "pgrn_value.h"

#ifdef PGRN_SUPPORT_OPTIONS
#	include <access/reloptions.h>
#endif

#include <groonga.h>

#ifdef PGRN_SUPPORT_OPTIONS
typedef struct PGrnOptions
{
	int32 vl_len_;
	int tokenizerOffset;
	int normalizerOffset;
} PGrnOptions;

static relopt_kind PGrnReloptionKind;

static grn_ctx *ctx = &PGrnContext;

PG_FUNCTION_INFO_V1(pgroonga_options);

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
#endif
}

void
PGrnApplyOptionValues(Relation index,
					  const char **tokenizerName,
					  const char **normalizerName)
{
#ifdef PGRN_SUPPORT_OPTIONS
	PGrnOptions *options;

	options = (PGrnOptions *) (index->rd_options);
	if (!options)
		return;

	*tokenizerName  = ((const char *) options) + options->tokenizerOffset;
	*normalizerName = ((const char *) options) + options->normalizerOffset;
#endif
}

#ifdef PGRN_SUPPORT_OPTIONS
/**
 * pgroonga.options() -- amoptions
 */
Datum
pgroonga_options(PG_FUNCTION_ARGS)
{
	Datum reloptions = PG_GETARG_DATUM(0);
	bool validate = PG_GETARG_BOOL(1);
	relopt_value *options;
	PGrnOptions *grnOptions;
	int nOptions;
	const relopt_parse_elt optionsMap[] = {
		{"tokenizer", RELOPT_TYPE_STRING,
		 offsetof(PGrnOptions, tokenizerOffset)},
		{"normalizer", RELOPT_TYPE_STRING,
		 offsetof(PGrnOptions, normalizerOffset)}
	};

	options = parseRelOptions(reloptions, validate, PGrnReloptionKind,
							  &nOptions);
	grnOptions = allocateReloptStruct(sizeof(PGrnOptions), options, nOptions);
	fillRelOptions(grnOptions, sizeof(PGrnOptions), options, nOptions,
				   validate, optionsMap, lengthof(optionsMap));
	pfree(options);

	PG_RETURN_BYTEA_P(grnOptions);
}
#endif
