#include <postgres.h>

#include <nodes/extensible.h>
#include <optimizer/pathnode.h>
#include <optimizer/paths.h>
#include <optimizer/restrictinfo.h>

#include "pgrn-custom-scan.h"
#include "pgroonga.h"

typedef struct PGrnScanState
{
	CustomScanState parent; /* must be first field */
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

	cpath = makeNode(CustomPath);
	cpath->path.pathtype = T_CustomScan;
	cpath->path.parent = rel;
	cpath->path.pathtarget = rel->reltarget;
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

	return (Node *) &(state->parent);
}

static void
PGrnBeginCustomScan(CustomScanState *cscanstate, EState *estate, int eflags)
{
}

static TupleTableSlot *
PGrnExecCustomScan(CustomScanState *node)
{
	return NULL;
}

static void
PGrnExplainCustomScan(CustomScanState *node, List *ancestors, ExplainState *es)
{
	ExplainPropertyText("PGroongaScan", "DEBUG", es);
}

static void
PGrnEndCustomScan(CustomScanState *node)
{
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
}
