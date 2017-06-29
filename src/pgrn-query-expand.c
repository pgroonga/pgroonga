#include "pgroonga.h"

#include "pgrn-compatible.h"

#include "pgrn-global.h"

#include <access/relscan.h>
#include <catalog/pg_type.h>
#include <utils/array.h>
#include <utils/builtins.h>
#include <utils/rel.h>
#include <utils/snapmgr.h>

static grn_ctx *ctx = &PGrnContext;

PGRN_FUNCTION_INFO_V1(pgroonga_query_expand);

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
	Datum queryDatum = PG_GETARG_DATUM(3);
	Datum tableOIDDatum;
	Oid tableOID;
	Relation table;
	TupleDesc desc;
	int i;
	Form_pg_attribute synonymsAttribute = NULL;
	Relation index;
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

	table = RelationIdGetRelation(tableOID);
	desc = RelationGetDescr(table);
	for (i = 1; i <= desc->natts; i++)
	{
		Form_pg_attribute attribute = desc->attrs[i - 1];
		if (strlen(attribute->attname.data) ==
			VARSIZE_ANY_EXHDR(synonymsColumnName) &&
			strncmp(attribute->attname.data,
					VARDATA_ANY(synonymsColumnName),
					VARSIZE_ANY_EXHDR(synonymsColumnName)) == 0)
		{
			synonymsAttribute = attribute;
			break;
		}
	}
	if (!synonymsAttribute)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_NAME),
				 errmsg("pgroonga: query_expand: "
						"synonyms column doesn't exist: <%s>.<%.*s>",
						DatumGetCString(tableNameDatum),
						(int)VARSIZE_ANY_EXHDR(synonymsColumnName),
						VARDATA_ANY(synonymsColumnName))));
	}

	index = PGrnFindTargetIndex(table,
								VARDATA_ANY(termColumnName),
								VARSIZE_ANY_EXHDR(termColumnName));
	if (!index)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_NAME),
				 errmsg("pgroonga: query_expand: "
						"index for term column doesn't exist: <%s>.<%.*s>",
						DatumGetCString(tableNameDatum),
						(int)VARSIZE_ANY_EXHDR(termColumnName),
						VARDATA_ANY(termColumnName))));
	}

	GRN_TEXT_INIT(&expandedQuery, 0);
	{
		Snapshot snapshot;
		IndexScanDesc scan;
		ScanKeyData scanKeys[1];
		HeapTuple tuple;
		Datum synonymsDatum;
		ArrayType *synonymsArray;
		bool isNULL;

		snapshot = GetActiveSnapshot();
		scan = index_beginscan(table, index, snapshot, 1, 0);
		ScanKeyInit(&(scanKeys[0]),
					1,
					BTEqualStrategyNumber,
					67, // F_TEXTEQ
					queryDatum);
		index_rescan(scan, scanKeys, 1, NULL, 0);
		tuple = index_getnext(scan, ForwardScanDirection);
		synonymsDatum = heap_getattr(tuple,
									 synonymsAttribute->attnum,
									 desc,
									 &isNULL);
		synonymsArray = DatumGetArrayTypeP(synonymsDatum);
		{
			int i, n;

			n = ARR_DIMS(synonymsArray)[0];
			for (i = 1; i <= n; i++)
			{
				Datum synonymDatum;
				bool isNULL;
				text *synonym;

				synonymDatum = array_ref(synonymsArray, 1, &i, -1,
										 synonymsAttribute->attlen,
										 synonymsAttribute->attbyval,
										 synonymsAttribute->attalign,
										 &isNULL);
				synonym = DatumGetTextP(synonymDatum);
				if (GRN_TEXT_LEN(&expandedQuery) > 0)
					GRN_TEXT_PUTS(ctx, &expandedQuery, " OR ");
				GRN_TEXT_PUT(ctx, &expandedQuery,
							 VARDATA_ANY(synonym),
							 VARSIZE_ANY_EXHDR(synonym));
			}
		}
		index_endscan(scan);

		index_close(index, NoLock);
	}

	RelationClose(table);

	{
		text *expandedQueryText;

		expandedQueryText =
			cstring_to_text_with_len(GRN_TEXT_VALUE(&expandedQuery),
									 GRN_TEXT_LEN(&expandedQuery));
		GRN_OBJ_FIN(ctx, &expandedQuery);

		PG_RETURN_TEXT_P(expandedQueryText);
	}
}

