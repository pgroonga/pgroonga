#include "pgroonga.h"

#include "pgrn-compatible.h"

#include "pgrn-global.h"
#include "pgrn-groonga.h"

#include <utils/builtins.h>
#ifdef PGRN_HAVE_VARATT_H
#	include <varatt.h>
#endif

static grn_ctx *ctx = &PGrnContext;
static struct PGrnBuffers *buffers = &PGrnBuffers;

PGDLLEXPORT PG_FUNCTION_INFO_V1(pgroonga_query_escape);

/**
 * pgroonga_query_escape(query text) : text
 */
Datum
pgroonga_query_escape(PG_FUNCTION_ARGS)
{
	const char *tag = "[query-escape]";
	text *query = PG_GETARG_TEXT_PP(0);
	text *escapedQuery;
	grn_obj *escapedQueryBuffer;

	escapedQueryBuffer = &(buffers->escape.escapedValue);
	GRN_BULK_REWIND(escapedQueryBuffer);
	grn_expr_syntax_escape_query(
		ctx, VARDATA_ANY(query), VARSIZE_ANY_EXHDR(query), escapedQueryBuffer);
	PGrnCheck("%s failed to escape: <%.*s>",
			  tag,
			  (int) VARSIZE_ANY_EXHDR(query),
			  VARDATA_ANY(query));

	escapedQuery = cstring_to_text_with_len(GRN_TEXT_VALUE(escapedQueryBuffer),
											GRN_TEXT_LEN(escapedQueryBuffer));
	PG_RETURN_TEXT_P(escapedQuery);
}
