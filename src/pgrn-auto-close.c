#include "pgroonga.h"

#include "pgrn-auto-close.h"
#include "pgrn-global.h"

static grn_ctx *ctx = &PGrnContext;
static grn_hash *usingIndexes = NULL;

void
PGrnInitializeAutoClose(void)
{
	usingIndexes = grn_hash_create(ctx,
								   NULL,
								   sizeof(Oid),
								   sizeof(Oid),
								   0);
}

void
PGrnFinalizeAutoClose(void)
{
	if (usingIndexes)
	{
		grn_hash_close(ctx, usingIndexes);
		usingIndexes = NULL;
	}
}

static void
PGrnAutoCloseCloseUnusedObjects(Oid nodeID)
{
	char name[GRN_TABLE_MAX_KEY_SIZE];
	grn_obj *table;

	/* TODO: Close lexicons, building sources and JSON related objects. */
	snprintf(name, sizeof(name),
			 PGrnSourcesTableNameFormat,
			 nodeID);
	table = grn_ctx_get(ctx, name, strlen(name));
	if (table) {
		grn_obj_close(ctx, table);
	}
}

void
PGrnAutoCloseUseIndex(Relation index)
{
	grn_id id;
	void *value;

	if (!usingIndexes)
		return;

	id = grn_hash_get(ctx,
					  usingIndexes,
					  &(index->rd_id),
					  sizeof(index->rd_id),
					  &value);
	if (id == GRN_ID_NIL)
	{
		id = grn_hash_add(ctx,
						  usingIndexes,
						  &(index->rd_id),
						  sizeof(index->rd_id),
						  &value,
						  NULL);
		if (id == GRN_ID_NIL)
			return;
		*((Oid *)value) = index->rd_node.relNode;
	}
	else
	{
		Oid currentNodeID = *((Oid *)value);
		if (index->rd_node.relNode == currentNodeID)
			return;
		PGrnAutoCloseCloseUnusedObjects(currentNodeID);
		*((Oid *)value) = index->rd_node.relNode;
	}
}
