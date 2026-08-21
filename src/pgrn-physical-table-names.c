#include "pgroonga.h"

#include "pgrn-groonga.h"
#include "pgrn-pg.h"

#include <catalog/pg_inherits.h>
#include <storage/lmgr.h>
#include <utils/builtins.h>
#include <utils/lsyscache.h>

PGDLLEXPORT PG_FUNCTION_INFO_V1(pgroonga_physical_table_names);

static bool
PGrnRelationIsPartitionedIndex(Relation relation)
{
	return RelationGetForm(relation)->relkind == RELKIND_PARTITIONED_INDEX;
}

static Oid
PGrnGetLogicalIndexOid(const char *tag, text *logicalIndexNameText)
{
	Oid logicalIndexOid =
		PGrnPGIndexNameToID(text_to_cstring(logicalIndexNameText));
	Relation logicalIndex = PGrnPGResolveIndexID(logicalIndexOid);
	if (!PGrnRelationIsPartitionedIndex(logicalIndex))
	{
		RelationClose(logicalIndex);
		PGrnCheckRC(GRN_INVALID_ARGUMENT,
					"%s the specified index is not partitioned: <%s>",
					tag,
					text_to_cstring(logicalIndexNameText));
	}
	RelationClose(logicalIndex);

	return logicalIndexOid;
}

/**
 * pgroonga_physical_table_names(logical_index_name text, argument_prefix text)
 * : text[]
 */
Datum
pgroonga_physical_table_names(PG_FUNCTION_ARGS)
{
	const char *tag = "[physical-table-names]";
	text *logicalIndexNameText = PG_GETARG_TEXT_PP(0);
	text *argumentNameText = PG_GETARG_TEXT_PP(1);

	if (VARSIZE_ANY_EXHDR(argumentNameText) >= GRN_TABLE_MAX_KEY_SIZE)
	{
		PGrnCheckRC(GRN_INVALID_ARGUMENT,
					"%s argument_prefix is too long: maximum length is <%d>, "
					"current length is <%zu>",
					tag,
					GRN_TABLE_MAX_KEY_SIZE - 1,
					VARSIZE_ANY_EXHDR(argumentNameText));
	}
	Oid logicalIndexOid = PGrnGetLogicalIndexOid(tag, logicalIndexNameText);

	LockRelationOid(logicalIndexOid, AccessShareLock);
	/**
	 * find_inheritance_children() here acquires an AccessShareLock and holds it
	 * until the end of the transaction. Therefore, the following operations on
	 * the child tables are blocked during this transaction.
	 * - DROP TABLE
	 * - TRUNCATE
	 * - REINDEX
	 * - CLUSTER
	 * - VACUUM FULL
	 * - REFRESH MATERIALIZED VIEW(not CONCURRENTLY)
	 * - ALTER INDEX
	 * - ALTER TABLE
	 */
	List *physicalIndexOids =
		find_inheritance_children(logicalIndexOid, AccessShareLock);
	if (!physicalIndexOids)
	{
		UnlockRelationOid(logicalIndexOid, AccessShareLock);
		GRN_LOG(ctx,
				GRN_LOG_WARNING,
				"%s <%s> has no children",
				tag,
				text_to_cstring(logicalIndexNameText));
		PG_RETURN_ARRAYTYPE_P(construct_empty_array(TEXTOID));
	}
	ListCell *cell;

	Oid physicalIndexOid;
	char tableNameBuffer[GRN_TABLE_MAX_KEY_SIZE];
	char argumentName[GRN_TABLE_MAX_KEY_SIZE];
	int nElements = list_length(physicalIndexOids) *
					2; // 2 is physical table name and argument prefix.
	Datum *physicalTableNamesDatum = palloc(nElements * sizeof(Datum));
	int i = 0, nPrefixArguments = 0;
	foreach (cell, physicalIndexOids)
	{
		snprintf(argumentName,
				 GRN_TABLE_MAX_KEY_SIZE,
				 "%s[%d].table",
				 text_to_cstring(argumentNameText),
				 nPrefixArguments++);
		physicalTableNamesDatum[i++] =
			PointerGetDatum(cstring_to_text(argumentName));

		physicalIndexOid = lfirst_oid(cell);
		PGrnGetSourcesTableNameFromOid(physicalIndexOid, tableNameBuffer);
		physicalTableNamesDatum[i++] =
			PointerGetDatum(cstring_to_text(tableNameBuffer));
	}
	UnlockRelationOid(logicalIndexOid, AccessShareLock);

	int dims[1] = {nElements};
	int lbs[1] = {1};
	PG_RETURN_ARRAYTYPE_P(construct_md_array(physicalTableNamesDatum,
											 NULL,
											 1,
											 dims,
											 lbs,
											 TEXTOID,
											 -1,
											 false,
											 TYPALIGN_INT));
}
