#include <postgres.h>

#if PG_VERSION_NUM >= 180000
#	include <commands/explain_format.h>
#endif
#include <access/heapam.h>
#include <catalog/index.h>
#include <executor/executor.h>
#include <nodes/extensible.h>
#include <nodes/nodeFuncs.h>
#include <optimizer/optimizer.h>
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
	Oid indexOID;
	List *scanKeySources;
	List *pathKeys;
	grn_table_cursor *tableCursor;
	grn_obj columns;
	grn_obj columnValue;
	PGrnSearchData searchData;
	grn_obj *searched;
	grn_obj *sorted;
	grn_obj *ctidAccessor;
	grn_obj *scoreAccessor;
} PGrnScanState;

bool PGrnCustomScanInitialized = false;
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

static Size PGrnEstimateDSMCustomScan(CustomScanState *customScanState,
									  ParallelContext *pcxt);
static void PGrnInitializeDSMCustomScan(CustomScanState *customScanState,
										ParallelContext *pcxt,
										void *coordinate);
static void PGrnReInitializeDSMCustomScan(CustomScanState *customScanState,
										  ParallelContext *pcxt,
										  void *coordinate);
static void PGrnInitializeWorkerCustomScan(CustomScanState *customScanState,
										   shm_toc *toc,
										   void *coordinate);

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

	.EstimateDSMCustomScan = PGrnEstimateDSMCustomScan,
	.InitializeDSMCustomScan = PGrnInitializeDSMCustomScan,
	.ReInitializeDSMCustomScan = PGrnReInitializeDSMCustomScan,
	.InitializeWorkerCustomScan = PGrnInitializeWorkerCustomScan,
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

static List *
PGrnConvertExprList(List *baserestrictinfo)
{
	List *exprList = NIL;
	ListCell *cell;
	foreach (cell, baserestrictinfo)
	{
		RestrictInfo *info = (RestrictInfo *) lfirst(cell);
		exprList = lappend(exprList, info->clause);
	}
	return exprList;
}

static int
PGrnGetIndexColumnAttributeNumber(IndexInfo *indexInfo, int tableAttnum)
{
	for (int i = 0; i < indexInfo->ii_NumIndexAttrs; i++)
	{
		if (indexInfo->ii_IndexAttrNumbers[i] == tableAttnum)
		{
			return i + 1;
		}
	}
	return 0;
}

static bool
PGrnOpInOpfamily(Relation index, Oid opNo)
{
	for (unsigned int i = 0; i < index->rd_att->natts; i++)
	{
		Oid opfamily = index->rd_opfamily[i];
		if (op_in_opfamily(opNo, opfamily))
		{
			return true;
		}
	}
	return false;
}

static List *
PGrnScanKeySourceMake(Relation index,
					  IndexInfo *indexInfo,
					  List *args,
					  Oid opNo,
					  int flags,
					  Oid collation,
					  Oid opFuncID)
{
	const char *tag = "pgroonga: [custom-scan][scankey-source][make]";
	Node *left = linitial(args);
	Node *right = lsecond(args);
	Var *column;
	Const *value;

	int attributeNumber = 0;
	int strategy;
	Oid leftType;
	Oid rightType;

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
		return NIL;
	}

	attributeNumber =
		PGrnGetIndexColumnAttributeNumber(indexInfo, column->varattno);
	if (attributeNumber == 0)
	{
		elog(DEBUG1,
			 "%s attribute number <%d> not found in index",
			 tag,
			 column->varattno);
		return NIL;
	}

	get_op_opfamily_properties(opNo,
							   index->rd_opfamily[attributeNumber - 1],
							   false,
							   &strategy,
							   &leftType,
							   &rightType);

	/*
	 * ScanKeySource will be set to `custom_private`.
	 * Since only a `Node` can be set to `custom_private`, a `List`, which is a
	 * `Node`, will be used to create the value.
	 */
	return list_make3(list_make3_int(flags, attributeNumber, strategy),
					  list_make3_oid(column->vartype, collation, opFuncID),
					  list_make1(value));
}

static int
PGrnScanKeySourceGetFlags(List *source)
{
	return linitial_int(linitial(source));
}

static int
PGrnScanKeySourceGetIndexAttrNumber(List *source)
{
	return lsecond_int(linitial(source));
}

static int
PGrnScanKeySourceGetIndexStrategy(List *source)
{
	return lthird_int(linitial(source));
}

static Oid
PGrnScanKeySourceGetVarType(List *source)
{
	return linitial_oid(lsecond(source));
}

static Oid
PGrnScanKeySourceGetCollation(List *source)
{
	return lsecond_oid(lsecond(source));
}

static Oid
PGrnScanKeySourceGetOpFuncID(List *source)
{
	return lthird_oid(lsecond(source));
}

static Const *
PGrnScanKeySourceGetValue(List *source)
{
	return linitial(lthird(source));
}

static List *
PGrnCustomPrivateMake(Oid indexOID, List *scanKeySources, List *pathKeys)
{
	// Only a `Node` can be set to `custom_private`.
	// See also the comments in PGrnScanKeySourceMake().
	return list_make3(list_make1_oid(indexOID), scanKeySources, pathKeys);
}

static Oid
PGrnCustomPrivateGetIndexOID(List *privateData)
{
	return linitial_oid(linitial(privateData));
}

static List *
PGrnCustomPrivateGetScanKeySources(List *privateData)
{
	return lsecond(privateData);
}

static List *
PGrnCustomPrivateGetPathKeys(List *privateData)
{
	return lthird(privateData);
}

static List *
PGrnCollectScanKeySources(Relation index, List *quals)
{
	const char *tag = "pgroonga: [custom-scan][scankey-sources][collect]";
	IndexInfo *indexInfo = BuildIndexInfo(index);
	List *scanKeySources = NIL;
	ListCell *cell;

	foreach (cell, quals)
	{
		Expr *expr = (Expr *) lfirst(cell);
		List *scanKeySource = NIL;

		if (IsA(expr, OpExpr))
		{
			OpExpr *opexpr = (OpExpr *) expr;
			if (!PGrnOpInOpfamily(index, opexpr->opno))
			{
				elog(DEBUG1,
					 "%s Unsupported OpExpr operator <%d>",
					 tag,
					 opexpr->opno);
				continue;
			}
			if (list_length(opexpr->args) != 2)
			{
				elog(DEBUG1,
					 "%s The number of opExpr->args is not 2. <%d>",
					 tag,
					 list_length(opexpr->args));
				continue;
			}

			scanKeySource = PGrnScanKeySourceMake(index,
												  indexInfo,
												  opexpr->args,
												  opexpr->opno,
												  0,
												  exprCollation((Node *) expr),
												  opexpr->opfuncid);
		}
		else if (IsA(expr, ScalarArrayOpExpr))
		{
			ScalarArrayOpExpr *opexpr = (ScalarArrayOpExpr *) expr;
			if (!PGrnOpInOpfamily(index, opexpr->opno))
			{
				elog(DEBUG1,
					 "%s Unsupported ScalarArrayOpExpr operator <%d>",
					 tag,
					 opexpr->opno);
				continue;
			}
			if (list_length(opexpr->args) != 2)
			{
				elog(DEBUG1,
					 "%s The number of scalarArrayOpExpr->args is not 2. <%d>",
					 tag,
					 list_length(opexpr->args));
				continue;
			}
			scanKeySource = PGrnScanKeySourceMake(index,
												  indexInfo,
												  opexpr->args,
												  opexpr->opno,
												  SK_SEARCHARRAY,
												  exprCollation((Node *) expr),
												  opexpr->opfuncid);
		}
		else
		{
			elog(DEBUG1, "%s Unsupported Expr <%d>", tag, nodeTag(expr));
			continue;
		}

		if (scanKeySource)
			scanKeySources = lappend(scanKeySources, scanKeySource);
	}
	pfree(indexInfo);
	return scanKeySources;
}

static bool
PGrnIndexContainColumn(Relation index, const char *name)
{
	TupleDesc tupdesc = RelationGetDescr(index);
	for (AttrNumber i = 0; i < tupdesc->natts; i++)
	{
		Form_pg_attribute attr = TupleDescAttr(tupdesc, i);
		if (strcmp(NameStr(attr->attname), name) == 0)
			return true;
	}
	return false;
}

static const char *
PGrnTableColumnName(Relation table, Var *var)
{
	Form_pg_attribute attr = TupleDescAttr(table->rd_att, var->varattno - 1);
	return NameStr(attr->attname);
}

static List *
PGrnIndexSortClauses(Relation table, Relation index, PlannerInfo *plannerInfo)
{
	List *indexSortClauses = NIL;
	ListCell *cell;
	foreach (cell, plannerInfo->parse->sortClause)
	{
		SortGroupClause *sortGroupClause = (SortGroupClause *) lfirst(cell);
		Expr *expr = (Expr *) get_sortgroupclause_expr(
			sortGroupClause, plannerInfo->parse->targetList);

		if (IsA(expr, Var))
		{
			Var *var = (Var *) expr;
			const char *name = PGrnTableColumnName(table, var);
			if (PGrnIndexContainColumn(index, name))
			{
				indexSortClauses = lappend(indexSortClauses, sortGroupClause);
			}
		}
	}
	return indexSortClauses;
}

static List *
PGrnChooseIndex(Relation table, PlannerInfo *plannerInfo, List *quals)
{
	// todo: Support pgroonga_condition() index specification.
	// todo: Implementation of the logic for choosing which index to use.
	ListCell *cell;
	List *indexes = NIL;

	if (!table)
		return NULL;

	indexes = RelationGetIndexList(table);
	foreach (cell, indexes)
	{
		Oid indexOID = lfirst_oid(cell);
		Relation index = RelationIdGetRelation(indexOID);
		List *scanKeySources = NIL;
		List *sortClauses = NIL;
		List *pathKeys = NIL;
		if (!PGrnIndexIsPGroonga(index))
		{
			RelationClose(index);
			continue;
		}
		scanKeySources = PGrnCollectScanKeySources(index, quals);
		sortClauses = PGrnIndexSortClauses(table, index, plannerInfo);
		RelationClose(index);
		if (!scanKeySources)
			continue;

		if (sortClauses)
		{
			pathKeys = make_pathkeys_for_sortclauses(
				plannerInfo, sortClauses, plannerInfo->parse->targetList);
		}
		return PGrnCustomPrivateMake(indexOID, scanKeySources, pathKeys);
	}
	return NIL;
}

static void
PGrnSetRelPathlistHook(PlannerInfo *root,
					   RelOptInfo *rel,
					   Index rti,
					   RangeTblEntry *rte)
{
	CustomPath *cpath;
	List *privateData = NIL;

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

	{
		// Do not custom scan when no index exists for PGroonga.
		Relation table = relation_open(rte->relid, AccessShareLock);
		if (table)
		{
			List *quals = PGrnConvertExprList(rel->baserestrictinfo);
			privateData = PGrnChooseIndex(table, root, quals);
			relation_close(table, AccessShareLock);
			if (!privateData)
			{
				return;
			}
		}
	}

	cpath = makeNode(CustomPath);
	cpath->path.pathtype = T_CustomScan;
	cpath->path.parent = rel;
	cpath->path.pathtarget = rel->reltarget;
	cpath->path.pathkeys = PGrnCustomPrivateGetPathKeys(privateData);

	// todo
	// Set appropriately according to the function to be executed.
	// cpath->path.parallel_aware = true;
	// cpath->path.parallel_safe = true;

	cpath->custom_private = privateData;

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
	cscan->custom_private = best_path->custom_private;

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
	PGrnScanState *state;

	if (!PGrnCustomScanInitialized)
		return NULL;

	state = (PGrnScanState *) newNode(sizeof(PGrnScanState), T_CustomScanState);
	state->parent.methods = &PGrnExecuteMethods;

	state->tableCursor = NULL;
	GRN_PTR_INIT(&(state->columns), GRN_OBJ_VECTOR, GRN_ID_NIL);
	GRN_VOID_INIT(&(state->columnValue));
	memset(&(state->searchData), 0, sizeof(state->searchData));
	state->searched = NULL;
	state->sorted = NULL;
	state->scoreAccessor = NULL;
	state->indexOID = PGrnCustomPrivateGetIndexOID(cscan->custom_private);
	state->scanKeySources =
		PGrnCustomPrivateGetScanKeySources(cscan->custom_private);
	state->pathKeys = PGrnCustomPrivateGetPathKeys(cscan->custom_private);

	return (Node *) &(state->parent);
}

static bool
PGrnIsIndexValueUsed(Relation index, const char *columnName, Oid typeID)
{
	if (typeID == JSONBOID)
		return false;
	return PGrnIndexContainColumn(index, columnName);
}

static void
PGrnSetTargetColumns(CustomScanState *customScanState,
					 Relation index,
					 grn_obj *targetTable)
{
	PGrnScanState *state = (PGrnScanState *) customScanState;
	Relation table = customScanState->ss.ss_currentRelation;
	ListCell *cell;

	foreach (cell, customScanState->ss.ps.plan->targetlist)
	{
		TargetEntry *entry = (TargetEntry *) lfirst(cell);
		if (IsA(entry->expr, Var))
		{
			Var *var = (Var *) entry->expr;
			const char *name = PGrnTableColumnName(table, var);
			if (PGrnIsIndexValueUsed(
					index, name, exprType((Node *) (entry->expr))))
			{
				grn_obj *column = PGrnLookupColumn(targetTable, name, ERROR);
				GRN_PTR_PUT(ctx, &(state->columns), column);
			}
			else
			{
				GRN_PTR_PUT(ctx, &(state->columns), NULL);
			}
		}
	}
}

static void
PGrnSearchBuildCustomScanConditions(CustomScanState *customScanState,
									Relation index)
{
	PGrnScanState *state = (PGrnScanState *) customScanState;
	ListCell *cell;
	foreach (cell, state->scanKeySources)
	{
		List *scanKeySource = (List *) lfirst(cell);
		int flags = PGrnScanKeySourceGetFlags(scanKeySource);
		int attributeNumber =
			PGrnScanKeySourceGetIndexAttrNumber(scanKeySource);
		int strategy = PGrnScanKeySourceGetIndexStrategy(scanKeySource);
		Oid varType = PGrnScanKeySourceGetVarType(scanKeySource);
		Oid collation = PGrnScanKeySourceGetCollation(scanKeySource);
		Oid opfuncid = PGrnScanKeySourceGetOpFuncID(scanKeySource);
		Const *value = PGrnScanKeySourceGetValue(scanKeySource);

		ScanKeyData key;
		ScanKeyEntryInitialize(&key,
							   flags,
							   attributeNumber,
							   strategy,
							   varType,
							   collation,
							   opfuncid,
							   value->constvalue);

		PGrnSearchBuildCondition(index, &key, &(state->searchData));
	}
}

static void
PGrnCustomScanSort(CustomScanState *customScanState)
{
	const char *tag = "pgroonga: [custom-scan][sort]";
	PGrnScanState *state = (PGrnScanState *) customScanState;
	Relation table = customScanState->ss.ss_currentRelation;
	int nPathKeys = list_length(state->pathKeys);
	grn_table_sort_key *sortKeys =
		(grn_table_sort_key *) palloc(sizeof(grn_table_sort_key) * nPathKeys);
	ListCell *cell;
	unsigned int nSortKeys = 0;
	foreach (cell, state->pathKeys)
	{
		PathKey *pathKey = (PathKey *) lfirst(cell);
		EquivalenceMember *member = linitial(pathKey->pk_eclass->ec_members);
		Expr *expr = (Expr *) (member->em_expr);
		if (IsA(expr, Var))
		{
			// Support only simple sorting by columns.
			Var *var = (Var *) expr;
			const char *name = PGrnTableColumnName(table, var);
			sortKeys[nSortKeys].key =
				grn_obj_column(ctx, state->searched, name, strlen(name));
			if (pathKey->pk_cmptype == COMPARE_LT)
				sortKeys[nSortKeys].flags = GRN_TABLE_SORT_ASC;
			else if (pathKey->pk_cmptype == COMPARE_GT)
				sortKeys[nSortKeys].flags = GRN_TABLE_SORT_DESC;
			nSortKeys++;
		}
		else
		{
			char *pathKeyStr = nodeToString(pathKey);
			elog(DEBUG1, "%s It is an invalid pathkey <%s>", tag, pathKeyStr);
			pfree(pathKeyStr);
			continue;
		}
	}

	state->sorted = grn_table_create(
		ctx, NULL, 0, NULL, GRN_OBJ_TABLE_NO_KEY, NULL, state->searched);
	grn_table_sort(
		ctx, state->searched, 0, -1, state->sorted, sortKeys, nSortKeys);

	for (unsigned int i = 0; i < nSortKeys; i++)
		grn_obj_unlink(ctx, sortKeys[i].key);
	pfree(sortKeys);
}

static void
PGrnBeginCustomScan(CustomScanState *customScanState,
					EState *estate,
					int eflags)
{
	PGrnScanState *state;
	Relation index;
	grn_obj *sourcesTable;

	if (!PGrnCustomScanInitialized)
		return;

	state = (PGrnScanState *) customScanState;
	index = RelationIdGetRelation(state->indexOID);
	sourcesTable = PGrnLookupSourcesTable(index, ERROR);
	PGrnSearchDataInit(&(state->searchData), index, sourcesTable);
	PGrnSearchBuildCustomScanConditions(customScanState, index);

	if (!state->searchData.isEmptyCondition)
	{
		grn_obj *targetTable = NULL;
		grn_table_selector *table_selector = grn_table_selector_open(
			ctx, sourcesTable, state->searchData.expression, GRN_OP_OR);
		grn_table_selector_set_fuzzy_max_distance_ratio(
			ctx, table_selector, state->searchData.fuzzyMaxDistanceRatio);

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

		if (state->pathKeys)
			PGrnCustomScanSort(customScanState);
		if (state->sorted)
			targetTable = state->sorted;
		else
			targetTable = state->searched;

		PGrnSetTargetColumns(customScanState, index, targetTable);
		state->tableCursor = grn_table_cursor_open(
			ctx, targetTable, NULL, 0, NULL, 0, 0, -1, GRN_CURSOR_ASCENDING);
		if (sourcesTable->header.type == GRN_TABLE_NO_KEY)
		{
			state->ctidAccessor =
				grn_obj_column(ctx,
							   targetTable,
							   PGrnSourcesCtidColumnName,
							   PGrnSourcesCtidColumnNameLength);
		}
		else
		{
			state->ctidAccessor = grn_obj_column(
				ctx, targetTable, GRN_COLUMN_NAME_KEY, GRN_COLUMN_NAME_KEY_LEN);
		}
		state->scoreAccessor = grn_obj_column(
			ctx, targetTable, GRN_COLUMN_NAME_SCORE, GRN_COLUMN_NAME_SCORE_LEN);
	}

	RelationClose(index);
	PGrnSearchDataFree(&(state->searchData));
	memset(&(state->searchData), 0, sizeof(state->searchData));
}

static TupleTableSlot *
PGrnExecCustomScan(CustomScanState *customScanState)
{
	const char *tag = "pgroonga: [custom-scan][exec]";
	PGrnScanState *state;
	Relation table;
	ItemPointerData ctid;
	grn_id id;
	uint64_t packedCtid;

	if (!PGrnCustomScanInitialized)
		return NULL;

	state = (PGrnScanState *) customScanState;
	table = customScanState->ss.ss_currentRelation;
	if (!state->tableCursor)
		return NULL;

	while (true)
	{
		id = grn_table_cursor_next(ctx, state->tableCursor);
		if (id == GRN_ID_NIL)
			return NULL;

		GRN_BULK_REWIND(&(state->columnValue));
		grn_obj_get_value(ctx, state->ctidAccessor, id, &(state->columnValue));
		packedCtid = GRN_UINT64_VALUE(&(state->columnValue));
		ctid = PGrnCtidUnpack(packedCtid);
		if (PGrnCtidIsAlive(table, &ctid))
			break;

		GRN_LOG(ctx,
				GRN_LOG_DEBUG,
				"%s[dead] <%s>: <(%u,%u),%u>",
				tag,
				table->rd_rel->relname.data,
				ctid.ip_blkid.bi_hi,
				ctid.ip_blkid.bi_lo,
				ctid.ip_posid);
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
					bool found = false;
					TupleTableSlot *tupleSlot = MakeSingleTupleTableSlot(
						RelationGetDescr(table), &TTSOpsBufferHeapTuple);
					found = table_tuple_fetch_row_version(
						table, &ctid, GetTransactionSnapshot(), tupleSlot);
					if (found)
					{
						Var *var = (Var *) entry->expr;
						bool isnull = false;
						slot->tts_values[ttsIndex] =
							slot_getattr(tupleSlot, var->varattno, &isnull);
						slot->tts_isnull[ttsIndex] = isnull;
					}
					else
					{
						slot->tts_values[ttsIndex] = 0;
						slot->tts_isnull[ttsIndex] = true;
					}
					ExecDropSingleTupleTableSlot(tupleSlot);
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

	if (!PGrnCustomScanInitialized)
		return;

	ExecClearTuple(customScanState->ss.ps.ps_ResultTupleSlot);
	state->indexOID = InvalidOid;
	state->scanKeySources = NIL;
	state->pathKeys = NIL;

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
	if (state->scoreAccessor)
	{
		grn_obj_close(ctx, state->scoreAccessor);
		state->scoreAccessor = NULL;
	}
	if (state->sorted)
	{
		grn_obj_close(ctx, state->sorted);
		state->sorted = NULL;
	}
	if (state->searched)
	{
		grn_obj_close(ctx, state->searched);
		state->searched = NULL;
	}
}

static void
PGrnReScanCustomScan(CustomScanState *node)
{
}

static Size
PGrnEstimateDSMCustomScan(CustomScanState *customScanState,
						  ParallelContext *pcxt)
{
	return 0;
}

static void
PGrnInitializeDSMCustomScan(CustomScanState *customScanState,
							ParallelContext *pcxt,
							void *coordinate)
{
}

static void
PGrnReInitializeDSMCustomScan(CustomScanState *customScanState,
							  ParallelContext *pcxt,
							  void *coordinate)
{
}

static void
PGrnInitializeWorkerCustomScan(CustomScanState *customScanState,
							   shm_toc *toc,
							   void *coordinate)
{
}

void
PGrnInitializeCustomScan(void)
{
	PreviousSetRelPathlistHook = set_rel_pathlist_hook;
	set_rel_pathlist_hook = PGrnSetRelPathlistHook;

	RegisterCustomScanMethods(&PGrnScanMethods);
	PGrnCustomScanInitialized = true;
}

void
PGrnFinalizeCustomScan(void)
{
	PGrnCustomScanInitialized = false;
	set_rel_pathlist_hook = PreviousSetRelPathlistHook;
}
