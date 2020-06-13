#include "pgroonga.h"

#include "pgrn-compatible.h"

#include "pgrn-pg.h"

#include <groonga.h>

#include <utils/builtins.h>

PGRN_FUNCTION_INFO_V1(pgroonga_index_column_name_name);
PGRN_FUNCTION_INFO_V1(pgroonga_index_column_name_index);

/**
 * pgroonga_index_column_name_name(indexName cstring, columnName text) : text
 */
Datum
pgroonga_index_column_name_name(PG_FUNCTION_ARGS)
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
	RelationClose(index);

	if (desc->natts <= i)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("pgroonga: index_column_name: nonexistent column is specified: "
						"<%.*s>",
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

	PG_RETURN_TEXT_P(tableName);
}

/**
 * pgroonga_index_column_name_index(indexName cstring, columnIndex int4) : text
 */
Datum
pgroonga_index_column_name_index(PG_FUNCTION_ARGS)
{
	const char *indexName = PG_GETARG_CSTRING(0);
	const int32 columnIndex = PG_GETARG_INT32(1);

	Oid indexID;
	Oid fileNodeID;
	char tableNameBuffer[GRN_TABLE_MAX_KEY_SIZE];
	text *tableName = NULL;

	int n_attributes = 0;
	{
		Relation index = PGrnPGResolveIndexName(indexName);
		TupleDesc desc = RelationGetDescr(index);
		n_attributes = desc->natts;
		RelationClose(index);
	}

	indexID = PGrnPGIndexNameToID(indexName);
	fileNodeID = PGrnPGIndexIDToFileNodeID(indexID);

	if (columnIndex < 0 || n_attributes <= columnIndex)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("pgroonga: index_column_name: column index must be 0..%d: %d",
						n_attributes - 1,
						columnIndex)));
	}
	else
	{
		snprintf(tableNameBuffer, sizeof(tableNameBuffer),
				 PGrnIndexColumnNameFormat,
				 fileNodeID, columnIndex);
		tableName = cstring_to_text(tableNameBuffer);
	}

	PG_RETURN_TEXT_P(tableName);
}
