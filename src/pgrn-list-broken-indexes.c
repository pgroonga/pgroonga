#include "pgroonga.h"

#include "pgrn-groonga.h"

#include <access/heapam.h>
#include <funcapi.h>
#include <miscadmin.h>
#include <utils/acl.h>
#include <utils/builtins.h>

PGDLLEXPORT PG_FUNCTION_INFO_V1(pgroonga_list_broken_indexes);

typedef struct
{
	Relation indexes;
	TableScanDesc scan;
} PGrnListBrokenIndexData;

static bool
PGrnTableHaveBrokenColumn(grn_obj *table)
{
	grn_hash *columns;
	bool isBroken = false;

	columns = grn_hash_create(
		ctx, NULL, sizeof(grn_id), 0, GRN_TABLE_HASH_KEY | GRN_HASH_TINY);
	PGrnCheck("failed to create columns container for broken checks "
			  "<%s>",
			  PGrnInspectName(table));

	grn_table_columns(ctx, table, "", 0, (grn_obj *) columns);
	PGrnCheck("failed to collect columns for broken checks: <%s>",
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
			isBroken = true;
			break;
		}
		if (grn_obj_is_corrupt(ctx, column))
		{
			isBroken = true;
			break;
		}
	}
	GRN_HASH_EACH_END(ctx, cursor);

	grn_hash_close(ctx, columns);
	return isBroken;
}

static bool
PGrnIsBrokenSources(Relation index)
{
	grn_obj *table = PGrnLookupSourcesTable(index, ERROR);
	if (grn_obj_is_locked(ctx, table))
	{
		return true;
	}
	if (grn_obj_is_corrupt(ctx, table))
	{
		return true;
	}
	return PGrnTableHaveBrokenColumn(table);
}

static bool
PGrnIsBrokenLexicon(Relation index)
{
	int i;
	for (i = 0; i < index->rd_att->natts; i++)
	{
		grn_obj *lexicon = PGrnLookupLexicon(index, i, ERROR);
		if (grn_obj_is_locked(ctx, lexicon))
		{
			return true;
		}
		if (grn_obj_is_corrupt(ctx, lexicon))
		{
			return true;
		}
		if (PGrnTableHaveBrokenColumn(lexicon))
		{
			return true;
		}
	}
	return false;
}

static bool
PGrnIndexIsBroken(Relation index)
{
	return PGrnIsBrokenSources(index) || PGrnIsBrokenLexicon(index);
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

		if (!PGrnIndexIsBroken(index))
		{
			RelationClose(index);
			continue;
		}

		name = PointerGetDatum(cstring_to_text(RelationGetRelationName(index)));
		RelationClose(index);
		SRF_RETURN_NEXT(context, name);
	}

	heap_endscan(data->scan);
	table_close(data->indexes, lock);
	SRF_RETURN_DONE(context);
}
