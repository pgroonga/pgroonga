#include "pgroonga.h"

#include "pgrn_global.h"
#include "pgrn_groonga.h"

#include <utils/builtins.h>

static grn_ctx *ctx = &PGrnContext;
static struct PGrnBuffers *buffers = &PGrnBuffers;

PG_FUNCTION_INFO_V1(pgroonga_escape);

/**
 * pgroonga.escape(target text, special_characters text = '+-<>~*()":') : text
 */
Datum
pgroonga_escape(PG_FUNCTION_ARGS)
{
	grn_rc rc = GRN_SUCCESS;
	text *target = PG_GETARG_TEXT_PP(0);
	text *escapedTarget;
	grn_obj *escapedTargetBuffer;

	escapedTargetBuffer = &(buffers->escape.escapedTarget);
	GRN_BULK_REWIND(escapedTargetBuffer);
	if (PG_NARGS() == 1)
	{
		rc = grn_expr_syntax_escape_query(ctx,
										  VARDATA_ANY(target),
										  VARSIZE_ANY_EXHDR(target),
										  escapedTargetBuffer);
	}
	else
	{
		text *specialCharacters = PG_GETARG_TEXT_PP(1);
		grn_obj *specialCharactersBuffer;

		specialCharactersBuffer = &(buffers->escape.specialCharacters);
		GRN_TEXT_SET(ctx,
					 specialCharactersBuffer,
					 VARDATA_ANY(specialCharacters),
					 VARSIZE_ANY_EXHDR(specialCharacters));
		GRN_TEXT_PUTC(ctx, specialCharactersBuffer, '\0');
		rc = grn_expr_syntax_escape(ctx,
									VARDATA_ANY(target),
									VARSIZE_ANY_EXHDR(target),
									GRN_TEXT_VALUE(specialCharactersBuffer),
									GRN_QUERY_ESCAPE,
									escapedTargetBuffer);
	}

	if (rc != GRN_SUCCESS) {
		ereport(ERROR,
				(errcode(PGrnRCToPgErrorCode(rc)),
				 errmsg("pgroonga: escape: failed to escape")));
	}

	escapedTarget = cstring_to_text_with_len(GRN_TEXT_VALUE(escapedTargetBuffer),
											 GRN_TEXT_LEN(escapedTargetBuffer));
	PG_RETURN_TEXT_P(escapedTarget);
}
