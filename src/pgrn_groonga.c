#include "pgroonga.h"

#include "pgrn_global.h"
#include "pgrn_groonga.h"

static grn_ctx *ctx = &PGrnContext;

const char *
PGrnInspect(grn_obj *object)
{
	grn_obj *buffer = &(PGrnBuffers.inspect);

	GRN_BULK_REWIND(buffer);
	grn_inspect(ctx, buffer, object);
	GRN_TEXT_PUTC(ctx, buffer, '\0');
	return GRN_TEXT_VALUE(buffer);
}
