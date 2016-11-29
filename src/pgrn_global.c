#include "pgrn_global.h"

#include <stdlib.h>

grn_ctx PGrnContext;
struct PGrnBuffers PGrnBuffers;

static grn_ctx *ctx = &PGrnContext;

void
PGrnInitializeBuffers(void)
{
	GRN_VOID_INIT(&(PGrnBuffers.general));
	GRN_TEXT_INIT(&(PGrnBuffers.path), 0);
	GRN_TEXT_INIT(&(PGrnBuffers.keyword), 0);
	GRN_TEXT_INIT(&(PGrnBuffers.pattern), 0);
	GRN_UINT64_INIT(&(PGrnBuffers.ctid), 0);
	GRN_FLOAT_INIT(&(PGrnBuffers.score), 0);
	GRN_RECORD_INIT(&(PGrnBuffers.sourceIDs), GRN_OBJ_VECTOR, GRN_ID_NIL);
	GRN_UINT64_INIT(&(PGrnBuffers.jsonbValueKeys), GRN_OBJ_VECTOR);
	GRN_UINT64_INIT(&(PGrnBuffers.walPosition), 0);
	GRN_VOID_INIT(&(PGrnBuffers.walValue));
	GRN_UINT32_INIT(&(PGrnBuffers.maxRecordSize), 0);
	GRN_UINT64_INIT(&(PGrnBuffers.walAppliedPosition), 0);
	GRN_TEXT_INIT(&(PGrnBuffers.escape.escapedValue), 0);
	GRN_TEXT_INIT(&(PGrnBuffers.escape.specialCharacters), 0);
	GRN_TEXT_INIT(&(PGrnBuffers.head), 0);
	GRN_TEXT_INIT(&(PGrnBuffers.body), 0);
	GRN_TEXT_INIT(&(PGrnBuffers.foot), 0);
	GRN_TEXT_INIT(&(PGrnBuffers.inspect), 0);
}

void
PGrnFinalizeBuffers(void)
{
	GRN_OBJ_FIN(ctx, &(PGrnBuffers.general));
	GRN_OBJ_FIN(ctx, &(PGrnBuffers.path));
	GRN_OBJ_FIN(ctx, &(PGrnBuffers.keyword));
	GRN_OBJ_FIN(ctx, &(PGrnBuffers.pattern));
	GRN_OBJ_FIN(ctx, &(PGrnBuffers.ctid));
	GRN_OBJ_FIN(ctx, &(PGrnBuffers.score));
	GRN_OBJ_FIN(ctx, &(PGrnBuffers.sourceIDs));
	GRN_OBJ_FIN(ctx, &(PGrnBuffers.jsonbValueKeys));
	GRN_OBJ_FIN(ctx, &(PGrnBuffers.walPosition));
	GRN_OBJ_FIN(ctx, &(PGrnBuffers.walValue));
	GRN_OBJ_FIN(ctx, &(PGrnBuffers.maxRecordSize));
	GRN_OBJ_FIN(ctx, &(PGrnBuffers.walAppliedPosition));
	GRN_OBJ_FIN(ctx, &(PGrnBuffers.escape.escapedValue));
	GRN_OBJ_FIN(ctx, &(PGrnBuffers.escape.specialCharacters));
	GRN_OBJ_FIN(ctx, &(PGrnBuffers.head));
	GRN_OBJ_FIN(ctx, &(PGrnBuffers.body));
	GRN_OBJ_FIN(ctx, &(PGrnBuffers.foot));
	GRN_OBJ_FIN(ctx, &(PGrnBuffers.inspect));
}

