#include "pgroonga.h"

#include "pgrn-global.h"

#include <stdlib.h>

struct PGrnBuffers PGrnBuffers;
int PGrnMatchEscalationThreshold = 0;

void
PGrnInitializeBuffers(void)
{
	GRN_VOID_INIT(&(PGrnBuffers.general));
	GRN_VOID_INIT(&(PGrnBuffers.generalElement));
	GRN_VOID_INIT(&(PGrnBuffers.cast));
	GRN_VOID_INIT(&(PGrnBuffers.castElement));
	GRN_TEXT_INIT(&(PGrnBuffers.path), 0);
	GRN_TEXT_INIT(&(PGrnBuffers.keyword), 0);
	GRN_TEXT_INIT(&(PGrnBuffers.pattern), 0);
	GRN_UINT64_INIT(&(PGrnBuffers.ctid), 0);
	GRN_FLOAT_INIT(&(PGrnBuffers.score), 0);
	/* We want to use GRN_RECORD_INIT() for source IDs but
	 * grn_inspect(ctx, sourceIDs) is crashed with Groonga 13.0.2 or
	 * older. */
	/* GRN_RECORD_INIT(&(PGrnBuffers.sourceIDs), GRN_OBJ_VECTOR, GRN_ID_NIL); */
	GRN_UINT32_INIT(&(PGrnBuffers.sourceIDs), GRN_OBJ_VECTOR);
	GRN_TEXT_INIT(&(PGrnBuffers.tokenizer), 0);
	GRN_TEXT_INIT(&(PGrnBuffers.normalizers), 0);
	GRN_TEXT_INIT(&(PGrnBuffers.tokenFilters), 0);
	GRN_TEXT_INIT(&(PGrnBuffers.plugins), GRN_OBJ_VECTOR);
	GRN_TEXT_INIT(&(PGrnBuffers.text), 0);
	GRN_TEXT_INIT(&(PGrnBuffers.texts), GRN_OBJ_VECTOR);
	GRN_UINT64_INIT(&(PGrnBuffers.jsonbValueKeys), GRN_OBJ_VECTOR);
	GRN_UINT8_INIT(&(PGrnBuffers.jsonbTokenStack), GRN_OBJ_VECTOR);
	GRN_UINT64_INIT(&(PGrnBuffers.walPosition), 0);
	GRN_VOID_INIT(&(PGrnBuffers.walValue));
	GRN_UINT32_INIT(&(PGrnBuffers.maxRecordSize), 0);
	GRN_UINT64_INIT(&(PGrnBuffers.walAppliedPosition), 0);
	GRN_BOOL_INIT(&(PGrnBuffers.isTargets), GRN_OBJ_VECTOR);
	GRN_TEXT_INIT(&(PGrnBuffers.escape.escapedValue), 0);
	GRN_TEXT_INIT(&(PGrnBuffers.escape.specialCharacters), 0);
	GRN_TEXT_INIT(&(PGrnBuffers.head), 0);
	GRN_TEXT_INIT(&(PGrnBuffers.body), 0);
	GRN_TEXT_INIT(&(PGrnBuffers.foot), 0);
	GRN_TEXT_INIT(&(PGrnBuffers.walBuffer), 0);
	GRN_TEXT_INIT(&(PGrnBuffers.sourceNames), GRN_OBJ_VECTOR);
	GRN_INT32_INIT(&(PGrnBuffers.sourceNameSizes), GRN_OBJ_VECTOR);
}

void
PGrnFinalizeBuffers(void)
{
	GRN_OBJ_FIN(ctx, &(PGrnBuffers.general));
	GRN_OBJ_FIN(ctx, &(PGrnBuffers.generalElement));
	GRN_OBJ_FIN(ctx, &(PGrnBuffers.cast));
	GRN_OBJ_FIN(ctx, &(PGrnBuffers.castElement));
	GRN_OBJ_FIN(ctx, &(PGrnBuffers.path));
	GRN_OBJ_FIN(ctx, &(PGrnBuffers.keyword));
	GRN_OBJ_FIN(ctx, &(PGrnBuffers.pattern));
	GRN_OBJ_FIN(ctx, &(PGrnBuffers.ctid));
	GRN_OBJ_FIN(ctx, &(PGrnBuffers.score));
	GRN_OBJ_FIN(ctx, &(PGrnBuffers.sourceIDs));
	GRN_OBJ_FIN(ctx, &(PGrnBuffers.tokenizer));
	GRN_OBJ_FIN(ctx, &(PGrnBuffers.normalizers));
	GRN_OBJ_FIN(ctx, &(PGrnBuffers.tokenFilters));
	GRN_OBJ_FIN(ctx, &(PGrnBuffers.plugins));
	GRN_OBJ_FIN(ctx, &(PGrnBuffers.jsonbValueKeys));
	GRN_OBJ_FIN(ctx, &(PGrnBuffers.jsonbTokenStack));
	GRN_OBJ_FIN(ctx, &(PGrnBuffers.text));
	GRN_OBJ_FIN(ctx, &(PGrnBuffers.texts));
	GRN_OBJ_FIN(ctx, &(PGrnBuffers.walPosition));
	GRN_OBJ_FIN(ctx, &(PGrnBuffers.walValue));
	GRN_OBJ_FIN(ctx, &(PGrnBuffers.maxRecordSize));
	GRN_OBJ_FIN(ctx, &(PGrnBuffers.walAppliedPosition));
	GRN_OBJ_FIN(ctx, &(PGrnBuffers.isTargets));
	GRN_OBJ_FIN(ctx, &(PGrnBuffers.escape.escapedValue));
	GRN_OBJ_FIN(ctx, &(PGrnBuffers.escape.specialCharacters));
	GRN_OBJ_FIN(ctx, &(PGrnBuffers.head));
	GRN_OBJ_FIN(ctx, &(PGrnBuffers.body));
	GRN_OBJ_FIN(ctx, &(PGrnBuffers.foot));
	GRN_OBJ_FIN(ctx, &(PGrnBuffers.walBuffer));
	GRN_OBJ_FIN(ctx, &(PGrnBuffers.sourceNames));
	GRN_OBJ_FIN(ctx, &(PGrnBuffers.sourceNameSizes));
}
