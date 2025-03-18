#include <postgres.h>

#include <nodes/extensible.h>
#include <optimizer/pathnode.h>
#include <optimizer/paths.h>
#include <optimizer/restrictinfo.h>

#include "pgrn-custom-scan.h"
#include "pgroonga.h"

typedef struct PGrnScanState
{
	CustomScanState customScanState; /* must be first field */
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
PGrnScanBeginCustomScan(CustomScanState *node, EState *estate, int eflags);
static TupleTableSlot *PGrnScanExecCustomScan(CustomScanState *node);
static void PGrnScanEndCustomScan(CustomScanState *node);
static void PGrnScanReScanCustomScan(CustomScanState *node);
static void PGrnScanExplainCustomScan(CustomScanState *node,
									  List *ancestors,
									  ExplainState *es);

const struct CustomPathMethods PGrnScanPathMethods = {
	.CustomName = "PGroongaScan",
	.PlanCustomPath = PGrnPlanCustomPath,
	.ReparameterizeCustomPathByChild = PGrnReparameterizeCustomPathByChild,
};

const struct CustomScanMethods PGrnScanScanMethods = {
	.CustomName = "PGroongaScan",
	.CreateCustomScanState = PGrnCreateCustomScanState,
};

const struct CustomExecMethods PGrnScanExecuteMethods = {
	.CustomName = "PGroongaScan",

	.BeginCustomScan = PGrnScanBeginCustomScan,
	.ExecCustomScan = PGrnScanExecCustomScan,
	.EndCustomScan = PGrnScanEndCustomScan,
	.ReScanCustomScan = PGrnScanReScanCustomScan,

	.ExplainCustomScan = PGrnScanExplainCustomScan,
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

	cpath = makeNode(CustomPath);
	cpath->path.pathtype = T_CustomScan;
	cpath->path.parent = rel;
	cpath->path.pathtarget = rel->reltarget;
	cpath->methods = &PGrnScanPathMethods;

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
	cscan->methods = &PGrnScanScanMethods;
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
	PGrnScanState *pgrnScanState =
		(PGrnScanState *) newNode(sizeof(PGrnScanState), T_CustomScanState);

	CustomScanState *cscanstate = &pgrnScanState->customScanState;
	cscanstate->methods = &PGrnScanExecuteMethods;

	return (Node *) cscanstate;
}

static void
PGrnScanBeginCustomScan(CustomScanState *cscanstate, EState *estate, int eflags)
{
}

static TupleTableSlot *
PGrnScanExecCustomScan(CustomScanState *node)
{
	return NULL;
}

static void
PGrnScanExplainCustomScan(CustomScanState *node,
						  List *ancestors,
						  ExplainState *es)
{
	ExplainPropertyText("PGroongaScan", "DEBUG", es);
}

static void
PGrnScanEndCustomScan(CustomScanState *node)
{
}

static void
PGrnScanReScanCustomScan(CustomScanState *node)
{
}

void
PGrnInitializeCustomScan(void)
{
	PreviousSetRelPathlistHook = set_rel_pathlist_hook;
	set_rel_pathlist_hook = PGrnSetRelPathlistHook;

	RegisterCustomScanMethods(&PGrnScanScanMethods);
}
