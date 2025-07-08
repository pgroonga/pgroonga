#include "pgroonga.h"

#include "pgrn-groonga.h"
#include "pgrn-query-expand.h"
#include "pgrn-trace-log.h"

#include <access/genam.h>
#include <access/heapam.h>
#include <access/relscan.h>
#include <access/tableam.h>
#include <catalog/pg_class.h>
#include <catalog/pg_operator.h>
#include <catalog/pg_type.h>
#include <utils/array.h>
#include <utils/builtins.h>
#include <utils/lsyscache.h>
#include <utils/rel.h>
#include <utils/snapmgr.h>

#include <groonga/plugin.h>

#define PGRN_EXPANDER_NAME "QueryExpanderPostgreSQL"
#define PGRN_EXPANDER_NAME_LENGTH (sizeof(PGRN_EXPANDER_NAME) - 1)

typedef struct
{
	Relation table;
	AttrNumber termAttributeNumber;
	Form_pg_attribute synonymsAttribute;
	Snapshot snapshot;
	IndexScanDesc scan;
	TupleTableSlot *slot;
	StrategyNumber scanStrategy;
	Oid scanOperator;
	RegProcedure scanProcedure;
} PGrnQueryExpandData;

static PGrnQueryExpandData currentData;
static const LOCKMODE lockMode = AccessShareLock;

PGDLLEXPORT PG_FUNCTION_INFO_V1(pgroonga_query_expand);

static grn_obj *
func_query_expander_postgresql(grn_ctx *ctx,
							   int nargs,
							   grn_obj **args,
							   grn_user_data *user_data)
{
	grn_rc rc = GRN_END_OF_DATA;
	grn_obj *term;
	grn_obj *expandedTerm;
	text *termText;
	ArrayType *termTexts;
	Datum scanKeyDatum = 0;
	ScanKeyData scanKeys[1];
	int nKeys = 1;
	TableScanDesc heapScan = NULL;
	HeapTuple tuple;
	int ith_synonyms = 0;

	term = args[0];
	expandedTerm = args[1];

	termText =
		cstring_to_text_with_len(GRN_TEXT_VALUE(term), GRN_TEXT_LEN(term));
	switch (currentData.scanOperator)
	{
	case TextEqualOperator:
		scanKeyDatum = PointerGetDatum(termText);
		break;
	case OID_ARRAY_CONTAINS_OP:
	{
		const size_t nElements = 1;
		Datum *elements;
		int dims[1];
		int lbs[1];

		elements = palloc(sizeof(Datum) * nElements);
		elements[0] = PointerGetDatum(termText);
		dims[0] = nElements;
		lbs[0] = 1;
		termTexts = construct_md_array(
			elements, NULL, 1, dims, lbs, TEXTOID, -1, false, 'i');
		scanKeyDatum = PointerGetDatum(termTexts);
		break;
	}
	default:
		break;
	}

	if (currentData.scan)
	{
		ScanKeyInit(&(scanKeys[0]),
					currentData.termAttributeNumber,
					currentData.scanStrategy,
					currentData.scanProcedure,
					scanKeyDatum);
		index_rescan(currentData.scan, scanKeys, nKeys, NULL, 0);
	}
	else
	{
		ScanKeyInit(&(scanKeys[0]),
					currentData.termAttributeNumber,
					InvalidStrategy,
					currentData.scanProcedure,
					scanKeyDatum);
		heapScan = table_beginscan(
			currentData.table, currentData.snapshot, nKeys, scanKeys);
	}

	while (true)
	{
		Datum synonymsDatum;

		if (currentData.scan)
		{
			if (index_getnext_slot(
					currentData.scan, ForwardScanDirection, currentData.slot))
				tuple = ExecFetchSlotHeapTuple(currentData.slot, false, NULL);
			else
				tuple = NULL;
		}
		else
		{
			tuple = heap_getnext(heapScan, ForwardScanDirection);
		}

		if (!tuple)
			break;

		{
			bool isNULL;
			synonymsDatum = heap_getattr(tuple,
										 currentData.synonymsAttribute->attnum,
										 RelationGetDescr(currentData.table),
										 &isNULL);
			if (isNULL)
				continue;
		}

		if (currentData.synonymsAttribute->atttypid == TEXTOID)
		{
			text *synonym;
			synonym = DatumGetTextP(synonymsDatum);

			if (ith_synonyms == 0)
				GRN_TEXT_PUTC(ctx, expandedTerm, '(');
			else
				GRN_TEXT_PUTS(ctx, expandedTerm, " OR ");

			GRN_TEXT_PUTC(ctx, expandedTerm, '(');
			GRN_TEXT_PUT(ctx,
						 expandedTerm,
						 VARDATA_ANY(synonym),
						 VARSIZE_ANY_EXHDR(synonym));
			GRN_TEXT_PUTC(ctx, expandedTerm, ')');
		}
		else
		{
			AnyArrayType *synonymsArray;
			int i, n;
			int nUsedSynonyms = 0;

			synonymsArray = DatumGetAnyArrayP(synonymsDatum);
			if (AARR_NDIM(synonymsArray) == 0)
				continue;

			n = AARR_DIMS(synonymsArray)[0];
			if (n == 0)
				continue;

			if (ith_synonyms == 0)
				GRN_TEXT_PUTC(ctx, expandedTerm, '(');
			else
				GRN_TEXT_PUTS(ctx, expandedTerm, " OR ");

			for (i = 1; i <= n; i++)
			{
				Datum synonymDatum;
				bool isNULL;
				text *synonym;

				synonymDatum =
					array_get_element(synonymsDatum,
									  1,
									  &i,
									  -1,
									  currentData.synonymsAttribute->attlen,
									  currentData.synonymsAttribute->attbyval,
									  currentData.synonymsAttribute->attalign,
									  &isNULL);
				if (isNULL)
				{
					/* TODO: Reduce log level to GRN_LOG_DEBUG
					 * in the next release. */
					GRN_LOG(ctx,
							GRN_LOG_NOTICE,
							"[query-expander-postgresql] NULL element exists");
					continue;
				}
				if (!synonymDatum)
				{
					/* TODO: Remove this in the next release. */
					GRN_LOG(ctx,
							GRN_LOG_NOTICE,
							"[query-expander-postgresql] "
							"NULL datum element exists");
					continue;
				}
				synonym = DatumGetTextP(synonymDatum);
				if (nUsedSynonyms >= 1)
					GRN_TEXT_PUTS(ctx, expandedTerm, " OR ");
				GRN_TEXT_PUTC(ctx, expandedTerm, '(');
				GRN_TEXT_PUT(ctx,
							 expandedTerm,
							 VARDATA_ANY(synonym),
							 VARSIZE_ANY_EXHDR(synonym));
				GRN_TEXT_PUTC(ctx, expandedTerm, ')');
				nUsedSynonyms++;
			}
		}

		ith_synonyms++;

		rc = GRN_SUCCESS;
	}
	if (ith_synonyms > 0)
		GRN_TEXT_PUTC(ctx, expandedTerm, ')');

	if (heapScan)
		heap_endscan(heapScan);

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
					func_query_expander_postgresql,
					NULL,
					NULL,
					0,
					NULL);
}

static Form_pg_attribute
PGrnFindSynonymsAttribute(PGrnQueryExpandData *data,
						  const char *tableName,
						  const char *columnName,
						  size_t columnNameSize,
						  const char *tag)
{
	TupleDesc desc;
	int i;

	PGRN_TRACE_LOG_ENTER();

	desc = RelationGetDescr(data->table);
	for (i = 1; i <= desc->natts; i++)
	{
		Form_pg_attribute attribute = TupleDescAttr(desc, i - 1);

		if (strlen(attribute->attname.data) != columnNameSize)
			continue;
		if (strncmp(attribute->attname.data, columnName, columnNameSize) != 0)
			continue;

		if (!(attribute->atttypid == TEXTOID ||
			  attribute->atttypid == TEXTARRAYOID))
		{
			PGrnCheckRC(GRN_INVALID_ARGUMENT,
						"%s synonyms column isn't text type nor text[] type: "
						"<%s>.<%.*s>",
						tag,
						tableName,
						(int) columnNameSize,
						columnName);
		}

		PGRN_TRACE_LOG_EXIT();
		return attribute;
	}

	PGrnCheckRC(GRN_INVALID_ARGUMENT,
				"%s synonyms column doesn't exist: <%s>.<%.*s>",
				tag,
				tableName,
				(int) columnNameSize,
				columnName);

	PGRN_TRACE_LOG_EXIT();
	return NULL;
}

static Relation
PGrnFindTermIndex(PGrnQueryExpandData *data,
				  const char *columnName,
				  size_t columnNameSize)
{
	Relation termIndex = InvalidRelation;
	Relation preferedIndex = InvalidRelation;
	List *indexOIDList;
	ListCell *cell;

	PGRN_TRACE_LOG_ENTER();

	indexOIDList = RelationGetIndexList(data->table);
	foreach (cell, indexOIDList)
	{
		Relation index = InvalidRelation;
		Oid indexOID = lfirst_oid(cell);
		int i;

		index = index_open(indexOID, lockMode);
		for (i = 1; i <= index->rd_att->natts; i++)
		{
			Form_pg_attribute attribute = TupleDescAttr(index->rd_att, i - 1);
			const char *name = attribute->attname.data;
			Oid opFamily;
			Oid opNo = InvalidOid;
			StrategyNumber strategy;

			if (strlen(name) != columnNameSize)
				continue;

			if (memcmp(name, columnName, columnNameSize) != 0)
				continue;

			opFamily = index->rd_opfamily[i - 1];
			switch (attribute->atttypid)
			{
			case TEXTOID:
				opNo = TextEqualOperator;
				break;
			case TEXTARRAYOID:
				opNo = OID_ARRAY_CONTAINS_OP;
				break;
			default:
				break;
			}
			if (!OidIsValid(opNo))
				continue;

			strategy = get_op_opfamily_strategy(opNo, opFamily);
			if (strategy == InvalidStrategy)
				continue;

			if (PGrnIndexIsPGroonga(index))
			{
				preferedIndex = index;
				data->termAttributeNumber = i;
				data->scanStrategy = strategy;
				data->scanOperator = opNo;
				break;
			}

			if (!RelationIsValid(termIndex))
			{
				termIndex = index;
				data->termAttributeNumber = i;
				data->scanStrategy = strategy;
				data->scanOperator = opNo;
			}

			break;
		}

		if (RelationIsValid(preferedIndex))
			break;

		if (termIndex == index)
			continue;

		index_close(index, lockMode);
		index = InvalidRelation;
	}
	list_free(indexOIDList);

	if (RelationIsValid(preferedIndex))
	{
		if (RelationIsValid(termIndex) && termIndex != preferedIndex)
			index_close(termIndex, lockMode);
		PGRN_TRACE_LOG_EXIT();
		return preferedIndex;
	}
	else
	{
		PGRN_TRACE_LOG_EXIT();
		return termIndex;
	}
}

static void
PGrnFindTermAttributeNumber(PGrnQueryExpandData *data,
							const char *tableName,
							const char *columnName,
							size_t columnNameSize,
							const char *tag)
{
	TupleDesc desc;
	int i;

	PGRN_TRACE_LOG_ENTER();

	desc = RelationGetDescr(data->table);
	for (i = 1; i <= desc->natts; i++)
	{
		Form_pg_attribute attribute = TupleDescAttr(desc, i - 1);

		if (strlen(attribute->attname.data) != columnNameSize)
			continue;
		if (strncmp(attribute->attname.data, columnName, columnNameSize) != 0)
			continue;

		switch (attribute->atttypid)
		{
		case TEXTOID:
			data->scanOperator = TextEqualOperator;
			break;
		case TEXTARRAYOID:
			data->scanOperator = OID_ARRAY_CONTAINS_OP;
			break;
		default:
			PGrnCheckRC(GRN_INVALID_ARGUMENT,
						"%s term column isn't text type nor text[] type: "
						"<%s>.<%.*s>",
						tag,
						tableName,
						(int) columnNameSize,
						columnName);
			break;
		}

		data->termAttributeNumber = attribute->attnum;
		PGRN_TRACE_LOG_EXIT();
		return;
	}

	PGrnCheckRC(GRN_INVALID_ARGUMENT,
				"%s term column doesn't exist: <%s>.<%.*s>",
				tag,
				tableName,
				(int) columnNameSize,
				columnName);

	data->termAttributeNumber = InvalidAttrNumber;
	PGRN_TRACE_LOG_EXIT();
	return;
}

static const char *
PGrnRelkindToString(char relkind)
{
	switch (relkind)
	{
	case RELKIND_RELATION:
		return "ordinary table";
	case RELKIND_INDEX:
		return "index";
	case RELKIND_SEQUENCE:
		return "sequence";
	case RELKIND_TOASTVALUE:
		return "toast";
	case RELKIND_VIEW:
		return "view";
	case RELKIND_MATVIEW:
		return "materialized view";
	case RELKIND_COMPOSITE_TYPE:
		return "composite type";
	case RELKIND_FOREIGN_TABLE:
		return "foreign table";
	case RELKIND_PARTITIONED_TABLE:
		return "partitioned table";
	case RELKIND_PARTITIONED_INDEX:
		return "partitioned index";
	default:
		return "unknown";
	}
}

static bool
PGrnRelationIsTable(Relation relation)
{
	return PGRN_RELKIND_HAS_TABLE_AM(RelationGetForm(relation)->relkind);
}

/**
 * pgroonga_query_expand(tableName cstring,
 *                       termColumnName text,
 *                       synonymsColumnName text,
 *                       query text) : text
 */
Datum
pgroonga_query_expand(PG_FUNCTION_ARGS)
{
	const char *tag = "[query-expand]";
	Datum tableNameDatum = PG_GETARG_DATUM(0);
	text *termColumnName = PG_GETARG_TEXT_PP(1);
	text *synonymsColumnName = PG_GETARG_TEXT_PP(2);
	text *query = PG_GETARG_TEXT_PP(3);
	Datum tableOIDDatum;
	Oid tableOID;
	Relation index;
	grn_obj expandedQuery;

	PGRN_TRACE_LOG_ENTER();

	tableOIDDatum = DirectFunctionCall1(regclassin, tableNameDatum);
	if (!OidIsValid(tableOIDDatum))
	{
		PGrnCheckRC(GRN_INVALID_ARGUMENT,
					"%s query_expand: unknown table name: <%s>",
					tag,
					DatumGetCString(tableNameDatum));
	}
	tableOID = DatumGetObjectId(tableOIDDatum);
	currentData.table = RelationIdGetRelation(tableOID);
	if (!PGrnRelationIsTable(currentData.table))
	{
		PGrnCheckRC(
			GRN_INVALID_ARGUMENT,
			"%s the specified table isn't table: <%s>: <%s>",
			tag,
			DatumGetCString(tableNameDatum),
			PGrnRelkindToString(RelationGetForm(currentData.table)->relkind));
	}
	currentData.synonymsAttribute =
		PGrnFindSynonymsAttribute(&currentData,
								  DatumGetCString(tableNameDatum),
								  VARDATA_ANY(synonymsColumnName),
								  VARSIZE_ANY_EXHDR(synonymsColumnName),
								  tag);

	index = PGrnFindTermIndex(&currentData,
							  VARDATA_ANY(termColumnName),
							  VARSIZE_ANY_EXHDR(termColumnName));
	if (!index)
		PGrnFindTermAttributeNumber(&currentData,
									DatumGetCString(tableNameDatum),
									VARDATA_ANY(termColumnName),
									VARSIZE_ANY_EXHDR(termColumnName),
									tag);

	currentData.snapshot = GetActiveSnapshot();
	if (index)
	{
		int nKeys = 1;
		int nOrderBys = 0;
		PGRN_TRACE_LOG("index_begin_scan");
		currentData.scan = pgrn_index_beginscan(currentData.table,
												index,
												currentData.snapshot,
												NULL,
												nKeys,
												nOrderBys);
		currentData.slot = table_slot_create(currentData.table, NULL);
	}
	else
	{
		currentData.scan = NULL;
		currentData.slot = NULL;
	}

	currentData.scanProcedure = get_opcode(currentData.scanOperator);

	GRN_TEXT_INIT(&expandedQuery, 0);
	{
		grn_expr_flags flags = PGRN_EXPR_QUERY_PARSE_FLAGS;
		PGRN_TRACE_LOG("grn_expr_syntax_expand_query");
		grn_expr_syntax_expand_query(
			ctx,
			VARDATA_ANY(query),
			VARSIZE_ANY_EXHDR(query),
			flags,
			grn_ctx_get(ctx, PGRN_EXPANDER_NAME, PGRN_EXPANDER_NAME_LENGTH),
			&expandedQuery);
	}
	if (currentData.scan)
	{
		PGRN_TRACE_LOG("index_close");
		ExecDropSingleTupleTableSlot(currentData.slot);
		index_endscan(currentData.scan);
		index_close(index, lockMode);
	}

	PGRN_TRACE_LOG("RelationClose");
	RelationClose(currentData.table);

	{
		text *expandedQueryText;

		expandedQueryText = cstring_to_text_with_len(
			GRN_TEXT_VALUE(&expandedQuery), GRN_TEXT_LEN(&expandedQuery));
		GRN_OBJ_FIN(ctx, &expandedQuery);

		PGRN_TRACE_LOG_EXIT();

		PG_RETURN_TEXT_P(expandedQueryText);
	}
}
