#include "pgroonga.h"

#include "pgrn-compatible.h"

#include "pgrn-pg.h"

#include <groonga.h>

#include <utils/builtins.h>

PGRN_FUNCTION_INFO_V1(pgroonga_index_column_name_string);
PGRN_FUNCTION_INFO_V1(pgroonga_index_column_name_int4);

/**
 * pgroonga_index_column_name_string(indexName cstring, columnName text) : text
 */
Datum
pgroonga_index_column_name_string(PG_FUNCTION_ARGS)
{
	const char *indexName = PG_GETARG_CSTRING(0);
	const text *columnName = PG_GETARG_TEXT_PP(1);
	const char *columnNameData = VARDATA_ANY(columnName);
	const unsigned int columnNameSize = VARSIZE_ANY_EXHDR(columnName);

	Oid indexID;
	Oid fileNodeID;
	char tableNameBuffer[GRN_TABLE_MAX_KEY_SIZE];
	text *tableName = NULL;

	Relation index = PGrnPGResolveIndexName(indexName);
	TupleDesc desc = RelationGetDescr(index);
	int i;

	indexID = PGrnPGIndexNameToID(indexName);
	fileNodeID = PGrnPGIndexIDToFileNodeID(indexID);

	for (i = 0; i < desc->natts; i++)
	{
		Form_pg_attribute attribute = TupleDescAttr(desc, i);
		const char *attributeName = attribute->attname.data;
		if (strlen(attributeName) != columnNameSize)
			continue;
		if (strncmp(attributeName, columnNameData, columnNameSize) == 0)
			break;
	}

	if (desc->natts <= i)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("pgroonga: an invlid value was specified for column name: %.*s",
						(const int)columnNameSize,
						columnNameData)));
	}
	else
	{
		snprintf(tableNameBuffer, sizeof(tableNameBuffer),
				 PGrnIndexColumnNameFormat,
				 fileNodeID, i);
		tableName = cstring_to_text(tableNameBuffer);
	}
	RelationClose(index);

	PG_RETURN_TEXT_P(tableName);
}

/**
 * pgroonga_index_column_name_int4(indexName cstring, ordinalNumber int4) : text
 */
Datum
pgroonga_index_column_name_int4(PG_FUNCTION_ARGS)
{
	const char *indexName = PG_GETARG_CSTRING(0);
	const int32 ordinalNumber = PG_GETARG_INT32(1);

	Oid indexID;
	Oid fileNodeID;
	char tableNameBuffer[GRN_TABLE_MAX_KEY_SIZE];
	text *tableName = NULL;

	Relation index = PGrnPGResolveIndexName(indexName);
	TupleDesc desc = RelationGetDescr(index);

	indexID = PGrnPGIndexNameToID(indexName);
	fileNodeID = PGrnPGIndexIDToFileNodeID(indexID);

	if (desc->natts <= ordinalNumber)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("pgroonga: an invlid value was specified for ordinalNumber: %d",
					 ordinalNumber)));
	}
	else
	{
		snprintf(tableNameBuffer, sizeof(tableNameBuffer),
				 PGrnIndexColumnNameFormat,
				 fileNodeID, ordinalNumber);
		tableName = cstring_to_text(tableNameBuffer);
	}
	RelationClose(index);

	PG_RETURN_TEXT_P(tableName);
}
