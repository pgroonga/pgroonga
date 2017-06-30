#include "pgroonga.h"

#include "pgrn-compatible.h"

#include "pgrn-global.h"

#include <access/relscan.h>
#include <catalog/pg_type.h>
#include <utils/array.h>
#include <utils/builtins.h>
#include <utils/rel.h>
#include <utils/snapmgr.h>

#include <groonga/plugin.h>

#define PGRN_EXPANDER_NAME			"QueryExpanderPostgreSQL"
#define PGRN_EXPANDER_NAME_LENGTH	(sizeof(PGRN_EXPANDER_NAME) - 1)

typedef struct {
	Relation table;
	Relation index;
	Form_pg_attribute synonymsAttribute;
	Snapshot snapshot;
	IndexScanDesc scan;
} PGrnQueryExpandData;

static grn_ctx *ctx = &PGrnContext;
static PGrnQueryExpandData currentData;

PGRN_FUNCTION_INFO_V1(pgroonga_query_expand);

static grn_obj *
func_query_expander_postgresql(grn_ctx *ctx,
							   int nargs,
							   grn_obj **args,
							   grn_user_data *user_data)
{
	grn_rc rc = GRN_END_OF_DATA;
	grn_obj *term;
	grn_obj *expandedTerm;
	ScanKeyData scanKeys[1];
	int nKeys = 1;
	int nOrderBys = 0;
	text *termText;
	HeapTuple tuple;
	Datum synonymsDatum;
	bool isNULL;

	term = args[0];
	expandedTerm = args[1];

	if (!currentData.scan)
	{
		currentData.scan = index_beginscan(currentData.table,
										   currentData.index,
										   currentData.snapshot,
										   nKeys,
										   nOrderBys);
	}
	termText = cstring_to_text_with_len(GRN_TEXT_VALUE(term),
										GRN_TEXT_LEN(term));
	ScanKeyInit(&(scanKeys[0]),
				1,
				BTEqualStrategyNumber,
				67, // F_TEXTEQ
				PointerGetDatum(termText));
	index_rescan(currentData.scan, scanKeys, nKeys, NULL, 0);
	tuple = index_getnext(currentData.scan, ForwardScanDirection);
	synonymsDatum = heap_getattr(tuple,
								 currentData.synonymsAttribute->attnum,
								 RelationGetDescr(currentData.table),
								 &isNULL);
	if (!isNULL) {
		ArrayType *synonymsArray;
		int i, n;

		synonymsArray = DatumGetArrayTypeP(synonymsDatum);
		n = ARR_DIMS(synonymsArray)[0];
		if (n > 1)
			GRN_TEXT_PUTC(ctx, expandedTerm, '(');
		for (i = 1; i <= n; i++)
		{
			Datum synonymDatum;
			bool isNULL;
			text *synonym;

			synonymDatum = array_ref(synonymsArray, 1, &i, -1,
									 currentData.synonymsAttribute->attlen,
									 currentData.synonymsAttribute->attbyval,
									 currentData.synonymsAttribute->attalign,
									 &isNULL);
			synonym = DatumGetTextP(synonymDatum);
			if (i > 1)
				GRN_TEXT_PUTS(ctx, expandedTerm, " OR ");
			if (n > 1)
				GRN_TEXT_PUTC(ctx, expandedTerm, '(');
			GRN_TEXT_PUT(ctx, expandedTerm,
						 VARDATA_ANY(synonym),
						 VARSIZE_ANY_EXHDR(synonym));
			if (n > 1)
				GRN_TEXT_PUTC(ctx, expandedTerm, ')');
		}
		if (n > 1)
			GRN_TEXT_PUTC(ctx, expandedTerm, ')');

		rc = GRN_SUCCESS;
	}

	{
		grn_obj *rc_object;

		rc_object = grn_plugin_proc_alloc(ctx, user_data, GRN_DB_INT32, 0);
		if (rc_object)
			GRN_INT32_SET(ctx, rc_object, rc);

		return rc_object;
	}
}

void
PGrnInitializeQueryExpand(void)
{
	grn_proc_create(ctx,
					PGRN_EXPANDER_NAME,
					PGRN_EXPANDER_NAME_LENGTH,
					GRN_PROC_FUNCTION,
					func_query_expander_postgresql, NULL, NULL,
					0, NULL);
}

static Form_pg_attribute
PGrnFindTargetAttribute(Relation table,
						const char *columnName,
						size_t columnNameSize)
{
	TupleDesc desc;
	int i;

	desc = RelationGetDescr(table);
	for (i = 1; i <= desc->natts; i++)
	{
		Form_pg_attribute attribute = desc->attrs[i - 1];

		if (strlen(attribute->attname.data) == columnNameSize &&
			strncmp(attribute->attname.data, columnName, columnNameSize) == 0)
		{
			return attribute;
		}
	}

	return NULL;
}

static Relation
PGrnFindTargetIndex(Relation table,
					const char *columnName,
					size_t columnNameSize)
{
	Relation index = InvalidRelation;
	List *indexOIDList;
	ListCell *cell;

	indexOIDList = RelationGetIndexList(table);
	foreach(cell, indexOIDList)
	{
		Oid indexOID = lfirst_oid(cell);
		bool isTargetIndex = false;
		int i;

		index = index_open(indexOID, NoLock);
		for (i = 1; i <= index->rd_att->natts; i++)
		{
			const char *name = index->rd_att->attrs[i - 1]->attname.data;
			if (strlen(name) == columnNameSize &&
				memcmp(name, columnName, columnNameSize) == 0)
			{
				isTargetIndex = true;
				break;
			}
		}

		if (isTargetIndex)
			break;

		index_close(index, NoLock);
		index = InvalidRelation;
	}
	list_free(indexOIDList);

	return index;
}

/**
 * pgroonga.query_expand(tableName cstring,
 *                       termColumnName text,
 *                       synonymsColumnName text,
 *                       query text) : text
 */
Datum
pgroonga_query_expand(PG_FUNCTION_ARGS)
{
	Datum tableNameDatum = PG_GETARG_DATUM(0);
	text *termColumnName = PG_GETARG_TEXT_PP(1);
	text *synonymsColumnName = PG_GETARG_TEXT_PP(2);
	text *query = PG_GETARG_TEXT_PP(3);
	Datum tableOIDDatum;
	Oid tableOID;
	grn_obj expandedQuery;

	tableOIDDatum = DirectFunctionCall1(regclassin, tableNameDatum);
	if (!OidIsValid(tableOIDDatum))
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_NAME),
				 errmsg("pgroonga: query_expand: unknown table name: <%s>",
						DatumGetCString(tableNameDatum))));
	}
	tableOID = DatumGetObjectId(tableOIDDatum);
	currentData.table = RelationIdGetRelation(tableOID);

	currentData.synonymsAttribute =
		PGrnFindTargetAttribute(currentData.table,
								VARDATA_ANY(synonymsColumnName),
								VARSIZE_ANY_EXHDR(synonymsColumnName));
	if (!currentData.synonymsAttribute)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_NAME),
				 errmsg("pgroonga: query_expand: "
						"synonyms column doesn't exist: <%s>.<%.*s>",
						DatumGetCString(tableNameDatum),
						(int)VARSIZE_ANY_EXHDR(synonymsColumnName),
						VARDATA_ANY(synonymsColumnName))));
	}

	currentData.index = PGrnFindTargetIndex(currentData.table,
											VARDATA_ANY(termColumnName),
											VARSIZE_ANY_EXHDR(termColumnName));
	if (!currentData.index)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_NAME),
				 errmsg("pgroonga: query_expand: "
						"index for term column doesn't exist: <%s>.<%.*s>",
						DatumGetCString(tableNameDatum),
						(int)VARSIZE_ANY_EXHDR(termColumnName),
						VARDATA_ANY(termColumnName))));
	}

	currentData.snapshot = GetActiveSnapshot();
	{
		int nKeys = 1;
		int nOrderBys = 0;
		currentData.scan = index_beginscan(currentData.table,
										   currentData.index,
										   currentData.snapshot,
										   nKeys,
										   nOrderBys);
	}
	GRN_TEXT_INIT(&expandedQuery, 0);
	grn_expr_syntax_expand_query(ctx,
								 VARDATA_ANY(query),
								 VARSIZE_ANY_EXHDR(query),
								 GRN_EXPR_SYNTAX_QUERY,
								 grn_ctx_get(ctx,
											 PGRN_EXPANDER_NAME,
											 PGRN_EXPANDER_NAME_LENGTH),
								 &expandedQuery);
	index_endscan(currentData.scan);
	index_close(currentData.index, NoLock);

	RelationClose(currentData.table);

	{
		text *expandedQueryText;

		expandedQueryText =
			cstring_to_text_with_len(GRN_TEXT_VALUE(&expandedQuery),
									 GRN_TEXT_LEN(&expandedQuery));
		GRN_OBJ_FIN(ctx, &expandedQuery);

		PG_RETURN_TEXT_P(expandedQueryText);
	}
}

