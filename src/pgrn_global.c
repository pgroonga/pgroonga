#include "pgrn_global.h"

grn_ctx PGrnContext;
struct PGrnBuffers PGrnBuffers;

static grn_ctx *ctx = &PGrnContext;

void
PGrnInitializeBuffers(void)
{
	GRN_TEXT_INIT(&(PGrnBuffers.inspect), 0);
}

void
PGrnFinalizeBuffers(void)
{
	GRN_OBJ_FIN(ctx, &(PGrnBuffers.inspect));
}

