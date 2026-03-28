#include "pgroonga.h"

#include "pgrn-groonga.h"
#include "pgrn-normalize.h"
#include "pgrn-string.h"

#include <utils/builtins.h>
#ifdef PGRN_HAVE_VARATT_H
#	include <varatt.h>
#endif

static grn_obj *lexicon = NULL;
static grn_obj normalizers;

PGDLLEXPORT PG_FUNCTION_INFO_V1(pgroonga_normalize);

void
PGrnInitializeNormalize(void)
{
	lexicon = grn_table_create(ctx,
							   NULL,
							   0,
							   NULL,
							   GRN_OBJ_TABLE_PAT_KEY,
							   grn_ctx_at(ctx, GRN_DB_SHORT_TEXT),
							   NULL);
	GRN_TEXT_INIT(&normalizers, 0);
}

void
PGrnFinalizeNormalize(void)
{
	GRN_OBJ_FIN(ctx, &normalizers);
	grn_obj_close(ctx, lexicon);
}

/**
 * pgroonga_normalize(target text, normalizers text) : text
 */
Datum
pgroonga_normalize(PG_FUNCTION_ARGS)
{
	text *target;
	grn_obj *string;
	unsigned int lengthInBytes;
	const char *normalized;
	text *normalizedTarget;
	const char *rawNormalizersData;
	size_t rawNormalizersLength;

	target = PG_GETARG_TEXT_PP(0);

	if (PG_NARGS() == 2)
	{
		text *rawNormalizers = PG_GETARG_TEXT_PP(1);
		rawNormalizersData = VARDATA_ANY(rawNormalizers);
		rawNormalizersLength = VARSIZE_ANY_EXHDR(rawNormalizers);
	}
	else
	{
		rawNormalizersData = PGRN_DEFAULT_NORMALIZERS;
		rawNormalizersLength = strlen(PGRN_DEFAULT_NORMALIZERS);
	}

	if (!(rawNormalizersLength == GRN_TEXT_LEN(&normalizers) &&
		  memcmp(rawNormalizersData,
				 GRN_TEXT_VALUE(&normalizers),
				 GRN_TEXT_LEN(&normalizers)) == 0))
	{
		GRN_BULK_REWIND(&normalizers);
		PGrnStringSubstituteVariables(
			rawNormalizersData, rawNormalizersLength, &normalizers);
		grn_obj_set_info(ctx, lexicon, GRN_INFO_NORMALIZER, &normalizers);
		PGrnCheck("normalize: failed to set normalizers: <%.*s>",
				  (int) GRN_TEXT_LEN(&normalizers),
				  GRN_TEXT_VALUE(&normalizers));
	}
	string = grn_string_open(
		ctx, VARDATA_ANY(target), VARSIZE_ANY_EXHDR(target), lexicon, 0);
	PGrnCheck("normalize: failed to open normalized string");

	grn_string_get_normalized(ctx, string, &normalized, &lengthInBytes, NULL);

	normalizedTarget = cstring_to_text_with_len(normalized, lengthInBytes);

	grn_obj_unlink(ctx, string);

	PG_RETURN_TEXT_P(normalizedTarget);
}
