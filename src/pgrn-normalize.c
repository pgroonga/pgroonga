#include "pgroonga.h"
#include "pgrn-compatible.h"
#include "pgrn-global.h"
#include "pgrn-groonga.h"

#include <utils/builtins.h>

static grn_ctx *ctx = &PGrnContext;

PGRN_FUNCTION_INFO_V1(pgroonga_normalize);

/**
 * pgroonga_normalize(target text, normalizerName text) : text
 */
Datum
pgroonga_normalize(PG_FUNCTION_ARGS)
{
	text *target;
	text *normalizerName;
	grn_obj *normalizer;
	grn_obj *string;
	unsigned int lengthInBytes;
	const char *normalized;
	text *ret;

	target = PG_GETARG_TEXT_PP(0);

	if (PG_NARGS() == 2) {
		normalizerName = PG_GETARG_TEXT_PP(1);
		normalizer = PGrnLookupWithSize(VARDATA_ANY(normalizerName),
										VARSIZE_ANY_EXHDR(normalizerName),
										ERROR);
	} else {
		normalizer = PGrnLookup(PGRN_DEFAULT_NORMALIZER, ERROR);
	}

	string = grn_string_open(ctx,
							 VARDATA_ANY(target),
							 VARSIZE_ANY_EXHDR(target),
							 normalizer,
							 0);
	if (!string)
		ereport(ERROR,
				(errcode(ERRCODE_SYSTEM_ERROR),
				errmsg("pgroonga: failed to open string")));

	grn_string_get_normalized(ctx,
							  string,
							  &normalized,
							  &lengthInBytes,
							  NULL);

	ret = cstring_to_text_with_len(normalized, lengthInBytes);

	grn_obj_unlink(ctx, string);

	PG_RETURN_TEXT_P(ret);
}
