#include "pgroonga.h"

#include "pgrn-compatible.h"

#include "pgrn-row-level-security.h"

#include <executor/execExpr.h>
#include <utils/portal.h>
#include <utils/rls.h>

bool PGrnEnableRLS = true;
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
/* EEOP_FUNCEXPR_STRICT_1 and EEOP_FUNCEXPR_STRICT_2 are added since
 * PostgreSQL 18. Please refer to
 * https://github.com/postgres/postgres/commit/d35d32d7112bc632c6a305e9dffdec0082bbdf00
 */
#ifdef PGRN_SUPPORT_FUNCEXPR_STRICT_1_2
		case EEOP_FUNCEXPR_STRICT_1:
		case EEOP_FUNCEXPR_STRICT_2:
#endif
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
/* defined in src/backend/nodeAppend.c */
#define INVALID_SUBPLAN_INDEX -1
		if (appendState->as_whichplan != INVALID_SUBPLAN_INDEX)
#undef INVALID_SUBPLAN_INDEX
		{
			PlanState *subState =
				appendState->appendplans[appendState->as_whichplan];
			if (subState)
				return PGrnFindTargetExprContext(subState, fcinfo);
		}
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

	if (!portal)
	{
		return false;
	}
	if (!portal->queryDesc)
	{
		/* EXPLAIN ANALYZE for sequential scan doesn't create
		   portal->queryDesc. */
		/* For safety */
		return true;
	}
	econtext = PGrnFindTargetExprContext(portal->queryDesc->planstate, fcinfo);
	if (!econtext)
	{
		/* For safety */
		return true;
	}
	tableOid = econtext->ecxt_scantuple->tts_tableOid;
	return PGrnCheckRLSEnabled(tableOid);
}

void
PGrnResetRLSEnabled(void)
{
	PGrnIsRLSEnabled = false;
}
