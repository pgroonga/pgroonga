#include <postgres.h>

#if PG_VERSION_NUM >= 180000
#	include <commands/explain_format.h>
#endif
#include <access/heapam.h>
#include <catalog/index.h>
#include <executor/executor.h>
#include <nodes/extensible.h>
#include <nodes/nodeFuncs.h>
#include <optimizer/pathnode.h>
#include <optimizer/paths.h>
#include <optimizer/restrictinfo.h>
#include <storage/bufmgr.h>
#include <utils/lsyscache.h>
#include <utils/snapmgr.h>

#include "pgroonga.h"

#include "pgrn-ctid.h"
#include "pgrn-custom-scan.h"
#include "pgrn-groonga.h"
#include "pgrn-search.h"

typedef struct PGrnScanState
{
	CustomScanState parent; /* must be first field */
	grn_table_cursor *tableCursor;
	grn_obj columns;
	grn_obj columnValue;
	PGrnSearchData searchData;
	grn_obj *searched;
	grn_obj *ctidAccessor;
	grn_obj *scoreAccessor;
} PGrnScanState;

static bool PGrnCustomScanEnabled = false;
static set_rel_pathlist_hook_type PreviousSetRelPathlistHook = NULL;

static void PGrnSetRelPathlistHook(PlannerInfo *root,
								   RelOptInfo *rel,
								   Index rti,
								   RangeTblEntry *rte);
static Plan *PGrnPlanCustomPath(PlannerInfo *root,
								RelOptInfo *rel,
								struct CustomPath *best_path,
								List *tlist,
								List *clauses,
								List *custom_plans);
static List *PGrnReparameterizeCustomPathByChild(PlannerInfo *root,
												 List *custom_private,
												 RelOptInfo *child_rel);
static Node *PGrnCreateCustomScanState(CustomScan *cscan);
static void
PGrnBeginCustomScan(CustomScanState *node, EState *estate, int eflags);
static TupleTableSlot *PGrnExecCustomScan(CustomScanState *node);
static void PGrnEndCustomScan(CustomScanState *node);
static void PGrnReScanCustomScan(CustomScanState *node);
static void
PGrnExplainCustomScan(CustomScanState *node, List *ancestors, ExplainState *es);

const struct CustomPathMethods PGrnPathMethods = {
	.CustomName = "PGroongaScan",
	.PlanCustomPath = PGrnPlanCustomPath,
	.ReparameterizeCustomPathByChild = PGrnReparameterizeCustomPathByChild,
};

const struct CustomScanMethods PGrnScanMethods = {
	.CustomName = "PGroongaScan",
	.CreateCustomScanState = PGrnCreateCustomScanState,
};

const struct CustomExecMethods PGrnExecuteMethods = {
	.CustomName = "PGroongaScan",

	.BeginCustomScan = PGrnBeginCustomScan,
	.ExecCustomScan = PGrnExecCustomScan,
	.EndCustomScan = PGrnEndCustomScan,
	.ReScanCustomScan = PGrnReScanCustomScan,

	.ExplainCustomScan = PGrnExplainCustomScan,
};

bool
PGrnCustomScanGetEnabled(void)
{
	return PGrnCustomScanEnabled;
}

void
PGrnCustomScanEnable(void)
{
	PGrnCustomScanEnabled = true;
}

void
PGrnCustomScanDisable(void)
{
	PGrnCustomScanEnabled = false;
}

static void
PGrnSetRelPathlistHook(PlannerInfo *root,
					   RelOptInfo *rel,
					   Index rti,
					   RangeTblEntry *rte)
{
	CustomPath *cpath;
	if (PreviousSetRelPathlistHook)
	{
		PreviousSetRelPathlistHook(root, rel, rti, rte);
	}

	if (!PGrnCustomScanGetEnabled())
	{
		return;
	}

	if (get_rel_relkind(rte->relid) != RELKIND_RELATION)
	{
		// First, support table scan.
		return;
	}

	cpath = makeNode(CustomPath);
	cpath->path.pathtype = T_CustomScan;
	cpath->path.parent = rel;
	cpath->path.pathtarget = rel->reltarget;

#if (PG_VERSION_NUM >= 150000)
	cpath->flags |= CUSTOMPATH_SUPPORT_PROJECTION;
#endif

	cpath->methods = &PGrnPathMethods;

	add_path(rel, &cpath->path);
}

static Plan *
PGrnPlanCustomPath(PlannerInfo *root,
				   RelOptInfo *rel,
				   struct CustomPath *best_path,
				   List *tlist,
				   List *clauses,
				   List *custom_plans)
{
	CustomScan *cscan = makeNode(CustomScan);
	cscan->methods = &PGrnScanMethods;
	cscan->scan.plan.qual = extract_actual_clauses(clauses, false);
	cscan->scan.plan.targetlist = tlist;
	cscan->scan.scanrelid = rel->relid;

	return &(cscan->scan.plan);
}

static List *
PGrnReparameterizeCustomPathByChild(PlannerInfo *root,
									List *custom_private,
									RelOptInfo *child_rel)
{
	return NIL;
}

static Node *
PGrnCreateCustomScanState(CustomScan *cscan)
{
	PGrnScanState *state =
		(PGrnScanState *) newNode(sizeof(PGrnScanState), T_CustomScanState);

	state->parent.methods = &PGrnExecuteMethods;

	state->tableCursor = NULL;
	GRN_PTR_INIT(&(state->columns), GRN_OBJ_VECTOR, GRN_ID_NIL);
	GRN_VOID_INIT(&(state->columnValue));
	memset(&(state->searchData), 0, sizeof(state->searchData));
	state->searched = NULL;
	state->scoreAccessor = NULL;

	return (Node *) &(state->parent);
}

static Relation
PGrnChooseIndex(Relation table)
{
	// todo: Support pgroonga_condition() index specification.
	// todo: Implementation of the logic for choosing which index to use.
	ListCell *cell;
	List *indexes;

	if (!table)
		return NULL;

	indexes = RelationGetIndexList(table);
	foreach (cell, indexes)
	{
		Oid indexId = lfirst_oid(cell);

		Relation index = RelationIdGetRelation(indexId);
		if (!PGrnIndexIsPGroonga(index))
		{
			RelationClose(index);
			continue;
		}
		return index;
	}
	return NULL;
}

static bool
PGrnIndexContainColumn(Relation index, const char *name)
{
	TupleDesc tupdesc = RelationGetDescr(index);
	for (unsigned int i = 0; i < tupdesc->natts; i++)
	{
		Form_pg_attribute attr = TupleDescAttr(tupdesc, i);
		if (strcmp(NameStr(attr->attname), name) == 0)
			return true;
	}
	return false;
}

static void
PGrnSetTargetColumns(CustomScanState *customScanState,
					 Relation index,
					 grn_obj *targetTable)
{
	PGrnScanState *state = (PGrnScanState *) customScanState;
	ListCell *cell;

	foreach (cell, customScanState->ss.ps.plan->targetlist)
	{
		TargetEntry *entry = (TargetEntry *) lfirst(cell);
		if (IsA(entry->expr, Var))
		{
			if (PGrnIndexContainColumn(index, entry->resname))
			{
				grn_obj *column =
					PGrnLookupColumn(targetTable, entry->resname, ERROR);
				GRN_PTR_PUT(ctx, &(state->columns), column);
			}
			else
			{
				GRN_PTR_PUT(ctx, &(state->columns), NULL);
			}
		}
	}
}

static int
PGrnGetIndexColumnAttributeNumber(Relation index, int tableAttnum)
{
	IndexInfo *indexInfo = BuildIndexInfo(index);
	for (unsigned int i = 0; i < indexInfo->ii_NumIndexAttrs; i++)
	{
		if (indexInfo->ii_IndexAttrNumbers[i] == tableAttnum)
		{
			pfree(indexInfo);
			return i + 1;
		}
	}
	pfree(indexInfo);
	return 0;
}

static void
PGrnSearchBuildCustomScanConditions(CustomScanState *customScanState,
									Relation index)
{
	PGrnScanState *state = (PGrnScanState *) customScanState;
	const char *tag = "pgroonga: [custom-scan][build-conditions]";

	List *quals = customScanState->ss.ps.plan->qual;
	ListCell *cell;
	foreach (cell, quals)
	{
		Expr *expr = (Expr *) lfirst(cell);
		OpExpr *opexpr;
		Node *left;
		Node *right;
		Var *column;
		Const *value;

		if (!IsA(expr, OpExpr))
		{
			elog(DEBUG1, "%s node type is not OpExpr <%d>", tag, nodeTag(expr));
			continue;
		}

		opexpr = (OpExpr *) expr;
		if (list_length(opexpr->args) != 2)
		{
			elog(DEBUG1,
				 "%s The number of arguments is not 2. <%d>",
				 tag,
				 list_length(opexpr->args));
			continue;
		}

		left = linitial(opexpr->args);
		right = lsecond(opexpr->args);

		if (nodeTag(left) == T_Var && nodeTag(right) == T_Const)
		{
			column = (Var *) left;
			value = (Const *) right;
		}
		else if (nodeTag(left) == T_Const && nodeTag(right) == T_Var)
		{
			column = (Var *) right;
			value = (Const *) left;
		}
		else
		{
			elog(DEBUG1,
				 "%s The arguments are not a pair of Var and Const. <%d op %d>",
				 tag,
				 nodeTag(left),
				 nodeTag(right));
			continue;
		}

		{
			int attributeNumber =
				PGrnGetIndexColumnAttributeNumber(index, column->varattno);
			Oid opfamily = index->rd_opfamily[attributeNumber - 1];
			int strategy;
			Oid leftType;
			Oid rightType;
			ScanKeyData scankey;

			get_op_opfamily_properties(opexpr->opno,
									   opfamily,
									   false,
									   &strategy,
									   &leftType,
									   &rightType);
			ScanKeyInit(&scankey,
						attributeNumber,
						strategy,
						opexpr->opfuncid,
						value->constvalue);
			PGrnSearchBuildCondition(index, &scankey, &(state->searchData));
		}
	}
}

static void
PGrnBeginCustomScan(CustomScanState *customScanState,
					EState *estate,
					int eflags)
{
	PGrnScanState *state = (PGrnScanState *) customScanState;
	Relation index = PGrnChooseIndex(customScanState->ss.ss_currentRelation);
	grn_obj *sourcesTable;

	if (!index)
		return;

	sourcesTable = PGrnLookupSourcesTable(index, ERROR);
	PGrnSearchDataInit(&(state->searchData), index, sourcesTable);
	PGrnSearchBuildCustomScanConditions(customScanState, index);

	if (!state->searchData.isEmptyCondition)
	{
		grn_table_selector *table_selector = grn_table_selector_open(
			ctx, sourcesTable, state->searchData.expression, GRN_OP_OR);

		state->searched =
			grn_table_create(ctx,
							 NULL,
							 0,
							 NULL,
							 GRN_OBJ_TABLE_HASH_KEY | GRN_OBJ_WITH_SUBREC,
							 sourcesTable,
							 0);
		grn_table_selector_select(ctx, table_selector, state->searched);
		grn_table_selector_close(ctx, table_selector);
		PGrnSetTargetColumns(customScanState, index, state->searched);
		state->tableCursor = grn_table_cursor_open(ctx,
												   state->searched,
												   NULL,
												   0,
												   NULL,
												   0,
												   0,
												   -1,
												   GRN_CURSOR_ASCENDING);
		if (sourcesTable->header.type == GRN_TABLE_NO_KEY)
		{
			state->ctidAccessor =
				grn_obj_column(ctx,
							   state->searched,
							   PGrnSourcesCtidColumnName,
							   PGrnSourcesCtidColumnNameLength);
		}
		else
		{
			state->ctidAccessor = grn_obj_column(ctx,
												 state->searched,
												 GRN_COLUMN_NAME_KEY,
												 GRN_COLUMN_NAME_KEY_LEN);
		}
		state->scoreAccessor = grn_obj_column(ctx,
											  state->searched,
											  GRN_COLUMN_NAME_SCORE,
											  GRN_COLUMN_NAME_SCORE_LEN);
	}

	RelationClose(index);
	PGrnSearchDataFree(&(state->searchData));
	memset(&(state->searchData), 0, sizeof(state->searchData));
}

static TupleTableSlot *
PGrnExecCustomScan(CustomScanState *customScanState)
{
	const char *tag = "pgroonga: [custom-scan][exec]";
	PGrnScanState *state = (PGrnScanState *) customScanState;
	Relation table = customScanState->ss.ss_currentRelation;
	ItemPointerData ctid;
	grn_id id;
	uint64_t packedCtid;

	if (!state->tableCursor)
		return NULL;

	id = grn_table_cursor_next(ctx, state->tableCursor);
	if (id == GRN_ID_NIL)
		return NULL;

	GRN_BULK_REWIND(&(state->columnValue));
	grn_obj_get_value(ctx, state->ctidAccessor, id, &(state->columnValue));
	packedCtid = GRN_UINT64_VALUE(&(state->columnValue));
	ctid = PGrnCtidUnpack(packedCtid);
	if (!PGrnCtidIsAlive(table, &ctid))
	{
		GRN_LOG(ctx,
				GRN_LOG_DEBUG,
				"%s[dead] <%s>: <%ld>",
				tag,
				table->rd_rel->relname.data,
				packedCtid);
		return NULL;
	}

	{
		TupleTableSlot *slot = customScanState->ss.ps.ps_ResultTupleSlot;
		unsigned int ttsIndex = 0;
		unsigned int varIndex = 0;
		ListCell *cell;

		ExecClearTuple(slot);
		// We might add state->targetlist instead of ss.ps.plan->targetlist.
		foreach (cell, customScanState->ss.ps.plan->targetlist)
		{
			TargetEntry *entry = (TargetEntry *) lfirst(cell);
			GRN_BULK_REWIND(&(state->columnValue));

			if (IsA(entry->expr, Var))
			{
				grn_obj *column = GRN_PTR_VALUE_AT(&(state->columns), varIndex);
				varIndex++;
				if (column)
				{
					Oid typeID = exprType((Node *) (entry->expr));
					grn_obj_get_value(ctx, column, id, &(state->columnValue));
					slot->tts_values[ttsIndex] =
						PGrnConvertToDatum(&(state->columnValue), typeID);
					// todo
					// If there are nullable columns, do not custom scan.
					// See also
					// https://github.com/pgroonga/pgroonga/pull/742#discussion_r2107937927
					slot->tts_isnull[ttsIndex] = false;
				}
				else
				{
					Var *var = (Var *) entry->expr;
					HeapTupleData tuple;
					Buffer buffer;
					bool found = false;
					bool isnull = false;
					ItemPointerCopy(&ctid, &tuple.t_self);
					tuple.t_tableOid = RelationGetRelid(table);
					found = pgrn_heap_fetch(
						table, GetActiveSnapshot(), &tuple, &buffer, false);
					if (found)
					{
						slot->tts_values[ttsIndex] = heap_getattr(
							&tuple, var->varattno, table->rd_att, &isnull);
						slot->tts_isnull[ttsIndex] = isnull;
					}
					else
					{
						slot->tts_values[ttsIndex] = (Datum) 0;
						slot->tts_isnull[ttsIndex] = true;
					}

					if (BufferIsValid(buffer))
						ReleaseBuffer(buffer);
				}
			}
			else if (IsA(entry->expr, FuncExpr))
			{
				FuncExpr *funcExpr = (FuncExpr *) (entry->expr);
				if (strcmp(get_func_name(funcExpr->funcid), "pgroonga_score") ==
					0)
				{
					// todo
					// Reject this function if the argument isn't
					// `(tableoid, ctid)` nor `(record)`.
					grn_obj_get_value(
						ctx, state->scoreAccessor, id, &(state->columnValue));
					slot->tts_values[ttsIndex] =
						PGrnConvertToDatum(&(state->columnValue), FLOAT8OID);
					slot->tts_isnull[ttsIndex] = false;
				}
				else
				{
					ExprContext *econtext =
						customScanState->ss.ps.ps_ExprContext;
					// todo
					// Initialization in BeginCustomScan.
					// See also
					// https://github.com/pgroonga/pgroonga/pull/783/files#r2374165154
					ExprState *state = ExecInitExpr((Expr *) funcExpr, NULL);
					bool isNull = false;
					ResetExprContext(econtext);
					slot->tts_values[ttsIndex] =
						ExecEvalExpr(state, econtext, &isNull);
					slot->tts_isnull[ttsIndex] = isNull;
				}
			}
			ttsIndex++;
		}
		return ExecStoreVirtualTuple(slot);
	}
}

static void
PGrnExplainCustomScan(CustomScanState *node, List *ancestors, ExplainState *es)
{
	// todo Add the necessary information when we find it.
}

static void
PGrnEndCustomScan(CustomScanState *customScanState)
{
	PGrnScanState *state = (PGrnScanState *) customScanState;
	unsigned int nTargetColumns;

	ExecClearTuple(customScanState->ss.ps.ps_ResultTupleSlot);

	if (state->tableCursor)
	{
		grn_table_cursor_close(ctx, state->tableCursor);
		state->tableCursor = NULL;
	}
	nTargetColumns = GRN_PTR_VECTOR_SIZE(&(state->columns));
	for (unsigned int i = 0; i < nTargetColumns; i++)
	{
		grn_obj *column = GRN_PTR_VALUE_AT(&(state->columns), i);
		grn_obj_unlink(ctx, column);
	}
	GRN_OBJ_FIN(ctx, &(state->columnValue));
	GRN_OBJ_FIN(ctx, &(state->columns));

	if (state->searchData.index)
	{
		PGrnSearchDataFree(&(state->searchData));
		memset(&(state->searchData), 0, sizeof(state->searchData));
	}
	if (state->ctidAccessor)
	{
		grn_obj_close(ctx, state->ctidAccessor);
		state->ctidAccessor = NULL;
	}
	if (state->searched)
	{
		grn_obj_close(ctx, state->searched);
		state->searched = NULL;
	}
	if (state->scoreAccessor)
	{
		grn_obj_close(ctx, state->scoreAccessor);
		state->scoreAccessor = NULL;
	}
}

static void
PGrnReScanCustomScan(CustomScanState *node)
{
}

void
PGrnInitializeCustomScan(void)
{
	PreviousSetRelPathlistHook = set_rel_pathlist_hook;
	set_rel_pathlist_hook = PGrnSetRelPathlistHook;

	RegisterCustomScanMethods(&PGrnScanMethods);
}

void
PGrnFinalizeCustomScan(void)
{
	set_rel_pathlist_hook = PreviousSetRelPathlistHook;
	// todo
	// Disable registered functions.
	// See also
	// https://github.com/pgroonga/pgroonga/pull/722#discussion_r2011284556
}
