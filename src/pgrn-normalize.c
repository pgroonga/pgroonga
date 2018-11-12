#include "pgroonga.h"
#include "pgrn-compatible.h"
#include "pgrn-global.h"
#include "pgrn-groonga.h"
#include "pgrn-normalize.h"

#include <utils/builtins.h>

static grn_ctx *ctx = &PGrnContext;
static grn_obj *lexicon = NULL;
static grn_obj normalizer;
static grn_obj normalizerBuffer;

PGRN_FUNCTION_INFO_V1(pgroonga_normalize);

void
PGrnInitializeNormalize(void)
{
	lexicon = grn_table_create(ctx, NULL, 0, NULL,
							   GRN_OBJ_TABLE_PAT_KEY,
							   grn_ctx_at(ctx, GRN_DB_SHORT_TEXT),
							   NULL);
	GRN_TEXT_INIT(&normalizer, GRN_OBJ_DO_SHALLOW_COPY);
	GRN_TEXT_INIT(&normalizerBuffer, GRN_OBJ_DO_SHALLOW_COPY);
}

void
PGrnFinalizeNormalize(void)
{
	GRN_OBJ_FIN(ctx, &normalizerBuffer);
	GRN_OBJ_FIN(ctx, &normalizer);
	grn_obj_close(ctx, lexicon);
}

/**
 * pgroonga_normalize(target text, normalizerName text) : text
 */
Datum
pgroonga_normalize(PG_FUNCTION_ARGS)
{
	text *target;
	grn_obj *string;
	unsigned int lengthInBytes;
	const char *normalized;
	text *normalizedTarget;

	target = PG_GETARG_TEXT_PP(0);

	if (PG_NARGS() == 2)
	{
		text *rawNormalizer;

		rawNormalizer = PG_GETARG_TEXT_PP(1);
		if (!(VARSIZE_ANY_EXHDR(rawNormalizer) == GRN_TEXT_LEN(&normalizer) &&
			  memcmp(VARDATA_ANY(rawNormalizer),
					 GRN_TEXT_VALUE(&normalizer),
					 GRN_TEXT_LEN(&normalizer)) == 0))
		{
			GRN_TEXT_SET(ctx,
						 &normalizerBuffer,
						 VARDATA_ANY(rawNormalizer),
						 VARSIZE_ANY_EXHDR(rawNormalizer));
			grn_obj_set_info(ctx,
							 lexicon,
							 GRN_INFO_NORMALIZER,
							 &normalizerBuffer);
			PGrnCheck("normalize: failed to set normalizer: <%.*s>",
					  (int) GRN_TEXT_LEN(&normalizerBuffer),
					  GRN_TEXT_VALUE(&normalizerBuffer));
			GRN_TEXT_SET(ctx,
						 &normalizer,
						 GRN_TEXT_VALUE(&normalizerBuffer),
						 GRN_TEXT_LEN(&normalizerBuffer));
		}
		string = grn_string_open(ctx,
								 VARDATA_ANY(target),
								 VARSIZE_ANY_EXHDR(target),
								 lexicon,
								 0);
	}
	else
	{
		string = grn_string_open(ctx,
								 VARDATA_ANY(target),
								 VARSIZE_ANY_EXHDR(target),
								 PGrnLookup(PGRN_DEFAULT_NORMALIZER, ERROR),
								 0);
	}
	PGrnCheck("normalize: failed to open normalized string");

	grn_string_get_normalized(ctx,
							  string,
							  &normalized,
							  &lengthInBytes,
							  NULL);

	normalizedTarget = cstring_to_text_with_len(normalized, lengthInBytes);

	grn_obj_unlink(ctx, string);

	PG_RETURN_TEXT_P(normalizedTarget);
}
