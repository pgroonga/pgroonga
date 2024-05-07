#include "pgroonga.h"

#include "pgrn-compatible.h"

#include "pgrn-global.h"
#include "pgrn-groonga.h"

#include <access/heapam.h>
#include <funcapi.h>
#include <miscadmin.h>
#include <utils/acl.h>
#include <utils/builtins.h>

PGDLLEXPORT PG_FUNCTION_INFO_V1(pgroonga_list_broken_indexes);

static grn_ctx *ctx = &PGrnContext;

typedef struct
{
	Relation indexes;
	TableScanDesc scan;
} PGrnListBrokenIndexData;

static bool
PGrnIsLockedColumn(grn_obj *table)
{
	grn_hash *columns;
	bool is_locked = false;

	columns = grn_hash_create(
		ctx, NULL, sizeof(grn_id), 0, GRN_TABLE_HASH_KEY | GRN_HASH_TINY);
	if (!columns)
	{
		PGrnCheck("failed to create columns container for checking locks: "
				  "<%s>",
				  PGrnInspectName(table));
	}
	grn_table_columns(ctx, table, "", 0, (grn_obj *) columns);
	PGrnCheck("failed to collect columns for checking locks: <%s>",
			  PGrnInspectName(table));

	GRN_HASH_EACH_BEGIN(ctx, columns, cursor, id)
	{
		grn_id *columnID;
		grn_obj *column;

		grn_hash_cursor_get_key(ctx, cursor, (void **) &columnID);
		column = grn_ctx_at(ctx, *columnID);
		if (!column)
			continue;
		if (grn_obj_is_locked(ctx, column))
		{
			is_locked = true;
			break;
		}
	}
	GRN_HASH_EACH_END(ctx, cursor);

	grn_hash_close(ctx, columns);
	return is_locked;
}

static bool
PGrnIsLockedSources(Relation index)
{
	grn_obj *table = PGrnLookupSourcesTable(index, ERROR);
	if (grn_obj_is_locked(ctx, table))
	{
		return true;
	}
	return PGrnIsLockedColumn(table);
}

static bool
PGrnIsLockedLexicon(Relation index)
{
	grn_obj *lexicon;
	int i;
	for (i = 0; i < index->rd_att->natts; i++)
	{
		lexicon = PGrnLookupLexicon(index, i, ERROR);
		if (grn_obj_is_locked(ctx, lexicon))
		{
			return true;
		}
		if (PGrnIsLockedColumn(lexicon))
		{
			return true;
		}
	}
	return false;
}

Datum
pgroonga_list_broken_indexes(PG_FUNCTION_ARGS)
{
	FuncCallContext *context;
	LOCKMODE lock = AccessShareLock;
	PGrnListBrokenIndexData *data;
	HeapTuple indexTuple;

	if (SRF_IS_FIRSTCALL())
	{
		MemoryContext oldContext;
		context = SRF_FIRSTCALL_INIT();
		oldContext = MemoryContextSwitchTo(context->multi_call_memory_ctx);

		data = palloc(sizeof(PGrnListBrokenIndexData));
		data->indexes = table_open(IndexRelationId, lock);
		data->scan = table_beginscan_catalog(data->indexes, 0, NULL);
		context->user_fctx = data;

		MemoryContextSwitchTo(oldContext);
	}

	context = SRF_PERCALL_SETUP();
	data = context->user_fctx;

	while ((indexTuple = heap_getnext(data->scan, ForwardScanDirection)))
	{
		Form_pg_index indexForm = (Form_pg_index) GETSTRUCT(indexTuple);
		Relation index;
		Datum name;

		if (!pgrn_pg_class_ownercheck(indexForm->indexrelid, GetUserId()))
			continue;

		index = RelationIdGetRelation(indexForm->indexrelid);
		if (!PGrnIndexIsPGroonga(index))
		{
			RelationClose(index);
			continue;
		}
		if (PGRN_RELKIND_HAS_PARTITIONS(index->rd_rel->relkind))
		{
			RelationClose(index);
			continue;
		}

		if (!PGrnIsLockedSources(index) && !PGrnIsLockedLexicon(index))
		{
			RelationClose(index);
			continue;
		}

		name = PointerGetDatum(cstring_to_text(RelationGetRelationName(index)));
		RelationClose(index);
		SRF_RETURN_NEXT(context, name);
	}

	// todo
	// Filtering broken(corrupt) indexes

	heap_endscan(data->scan);
	table_close(data->indexes, lock);
	SRF_RETURN_DONE(context);
}
