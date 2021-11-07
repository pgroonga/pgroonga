#include "pgroonga.h"

#include "pgrn-compatible.h"

#include "pgrn-row-level-security.h"

#include <executor/execExpr.h>
#include <utils/portal.h>
#include <utils/rls.h>

bool PGrnIsRLSEnabled = false;

bool
PGrnCheckRLSEnabled(Oid relationID)
{
	PGrnIsRLSEnabled =
		(check_enable_rls(relationID, InvalidOid, true) == RLS_ENABLED);
	return PGrnIsRLSEnabled;
}

static inline bool
PGrnIsTargetPlanState(PlanState *state, FunctionCallInfo fcinfo)
{
	ExprState *estate;
	int i;

	if (!state->ps_ExprContext)
		return false;
	if (!state->ps_ExprContext->ecxt_scantuple)
		return false;

	estate = state->qual;
	if (!estate)
		return false;

	for (i = 0; i < estate->steps_len; i++)
	{
		struct ExprEvalStep *step = &(estate->steps[i]);
		switch (ExecEvalStepOp(estate, step))
		{
		case EEOP_FUNCEXPR:
		case EEOP_FUNCEXPR_STRICT:
			if (step->d.func.fcinfo_data == fcinfo)
				return true;
			break;
		default:
			break;
		}
	}

	return false;
}

static inline ExprContext *
PGrnFindTargetExprContext(PlanState *state, FunctionCallInfo fcinfo)
{
	ExprContext *econtext;

	if (PGrnIsTargetPlanState(state, fcinfo))
		return state->ps_ExprContext;

	if (innerPlanState(state))
	{
		econtext = PGrnFindTargetExprContext(innerPlanState(state), fcinfo);
		if (econtext)
			return econtext;
	}

	if (outerPlanState(state))
	{
		econtext = PGrnFindTargetExprContext(outerPlanState(state), fcinfo);
		if (econtext)
			return econtext;
	}

	switch (state->type)
	{
	case T_AppendState:
	{
		AppendState *appendState = castNode(AppendState, state);
		PlanState *subState =
			appendState->appendplans[appendState->as_whichplan];
		if (subState)
			return PGrnFindTargetExprContext(subState, fcinfo);
		break;
	}
	default:
		break;
	}

	return NULL;
}

bool
PGrnCheckRLSEnabledSeqScan(FunctionCallInfo fcinfo)
{
	Portal portal = GetPortalByName("");
	ExprContext *econtext;
	Oid tableOid;

	if (!portal) {
		return false;
	}
	if (!(portal->queryDesc)) {/* For EXPLAIN ANALYZE */
		/* If portal->queryDesc is NULL, PostgreSQL's Executor is not active. */
		return false;
	}
	econtext = PGrnFindTargetExprContext(portal->queryDesc->planstate, fcinfo);
	if (!econtext) {
		/* For safety */
		return true;
	}
#ifdef PGRN_HAVE_TUPLE_TABLE_SLOT_TABLE_OID
	tableOid = econtext->ecxt_scantuple->tts_tableOid;
#else
	tableOid = econtext->ecxt_scantuple->tts_tuple->t_tableOid;
#endif
	return PGrnCheckRLSEnabled(tableOid);
}

void
PGrnResetRLSEnabled(void)
{
	PGrnIsRLSEnabled = false;
}
