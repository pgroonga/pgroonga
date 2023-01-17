#include "pgroonga.h"

#include "pgrn-compatible.h"
#include "pgrn-groonga.h"

#include "pgrn-pg.h"

#include <groonga.h>

#include <utils/builtins.h>
#ifdef PGRN_HAVE_VARATT_H
#	include <varatt.h>
#endif

PGDLLEXPORT PG_FUNCTION_INFO_V1(pgroonga_index_column_name_name);
PGDLLEXPORT PG_FUNCTION_INFO_V1(pgroonga_index_column_name_index);

/**
 * pgroonga_index_column_name_name(indexName cstring, columnName text) : text
 */
Datum
pgroonga_index_column_name_name(PG_FUNCTION_ARGS)
{
	const char *tag = "[index-column-name][name]";
	const char *indexName = PG_GETARG_CSTRING(0);
	const text *columnName = PG_GETARG_TEXT_PP(1);
	const char *columnNameData = VARDATA_ANY(columnName);
	const unsigned int columnNameSize = VARSIZE_ANY_EXHDR(columnName);

	int i;

	{
		Relation index = PGrnPGResolveIndexName(indexName);
		TupleDesc desc = RelationGetDescr(index);

		for (i = 0; i < desc->natts; i++)
		{
			Form_pg_attribute attribute = TupleDescAttr(desc, i);
			const char *attributeName = NameStr(attribute->attname);
			if (strlen(attributeName) != columnNameSize)
				continue;
			if (strncmp(attributeName, columnNameData, columnNameSize) == 0)
				break;
		}
		RelationClose(index);

		if (i == desc->natts)
		{
			PGrnCheckRC(GRN_INVALID_ARGUMENT,
						"%s nonexistent column is specified: <%.*s>",
						tag,
						(const int) columnNameSize,
						columnNameData);
		}
	}

	{
		Oid indexID;
		Oid fileNodeID;
		char indexColumnNameBuffer[GRN_TABLE_MAX_KEY_SIZE];
		text *indexColumnName;

		indexID = PGrnPGIndexNameToID(indexName);
		fileNodeID = PGrnPGIndexIDToFileNodeID(indexID);
		snprintf(indexColumnNameBuffer, sizeof(indexColumnNameBuffer),
				 PGrnIndexColumnNameFormat,
				 fileNodeID, i);
		indexColumnName = cstring_to_text(indexColumnNameBuffer);
		PG_RETURN_TEXT_P(indexColumnName);
	}
}

/**
 * pgroonga_index_column_name_index(indexName cstring, columnIndex int4) : text
 */
Datum
pgroonga_index_column_name_index(PG_FUNCTION_ARGS)
{
	const char *tag = "[index-column-name][index]";
	const char *indexName = PG_GETARG_CSTRING(0);
	const int32 columnIndex = PG_GETARG_INT32(1);

	{
		int n_attributes = 0;
		{
			Relation index = PGrnPGResolveIndexName(indexName);
			TupleDesc desc = RelationGetDescr(index);
			n_attributes = desc->natts;
			RelationClose(index);
		}

		if (columnIndex < 0 || n_attributes <= columnIndex)
		{
			PGrnCheckRC(GRN_INVALID_ARGUMENT,
						"%s column index must be 0..%d: %d",
						tag,
						n_attributes - 1,
						columnIndex);
		}
	}

	{
		Oid indexID;
		Oid fileNodeID;
		char indexColumnNameBuffer[GRN_TABLE_MAX_KEY_SIZE];
		text *indexColumnName;

		indexID = PGrnPGIndexNameToID(indexName);
		fileNodeID = PGrnPGIndexIDToFileNodeID(indexID);
		snprintf(indexColumnNameBuffer, sizeof(indexColumnNameBuffer),
				 PGrnIndexColumnNameFormat,
				 fileNodeID, columnIndex);
		indexColumnName = cstring_to_text(indexColumnNameBuffer);
		PG_RETURN_TEXT_P(indexColumnName);
	}
}
