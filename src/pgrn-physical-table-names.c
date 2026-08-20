#include "pgroonga.h"

#include "pgrn-groonga.h"
#include "pgrn-pg.h"
#include "pgrn-physical-table-names.h"

#include <catalog/pg_inherits.h>
#include <storage/lmgr.h>
#include <utils/builtins.h>
#include <utils/lsyscache.h>

PGDLLEXPORT PG_FUNCTION_INFO_V1(pgroonga_physical_table_names);

void
PGrnInitializePhysicalTableNames(void)
{
	;
}

void
PGrnFinalizePhysicalTableNames(void)
{
	;
}

static bool
PGrnRelationIsPartitionedIndex(Relation relation)
{
	return RelationGetForm(relation)->relkind == RELKIND_PARTITIONED_INDEX;
}

static Oid
PGrnGetLogicalIndexOid(const char *tag, text *logical_index_name_text)
{
	Oid logical_index_oid =
		PGrnPGIndexNameToID(text_to_cstring(logical_index_name_text));
	Relation logical_index = PGrnPGResolveIndexID(logical_index_oid);
	if (!PGrnRelationIsPartitionedIndex(logical_index))
	{
		RelationClose(logical_index);
		PGrnCheckRC(GRN_INVALID_ARGUMENT,
					"%s the specified index is not partitioned: <%s>",
					tag,
					text_to_cstring(logical_index_name_text));
	}
	RelationClose(logical_index);

	return logical_index_oid;
}

static void
PGrnGetSourcesTableNameFromOid(Oid oid, char tableName[GRN_TABLE_MAX_KEY_SIZE])
{
	Oid fileNodeID = PGrnPGIndexIDToFileNodeID(oid);
	snprintf(tableName,
			 GRN_TABLE_MAX_KEY_SIZE,
			 PGrnSourcesTableNameFormat,
			 fileNodeID);
}

/**
 * pgroonga_physical_table_names(logical_index_name text, argument_prefix text)
 * : text[]
 */
Datum
pgroonga_physical_table_names(PG_FUNCTION_ARGS)
{
	const char *tag = "[physical-table-names]";
	text *logical_index_name_text = PG_GETARG_TEXT_PP(0);

	Oid logical_index_oid =
		PGrnGetLogicalIndexOid(tag, logical_index_name_text);

	LockRelationOid(logical_index_oid, AccessShareLock);
	List *physical_index_oids =
		find_inheritance_children(logical_index_oid, AccessShareLock);
	if (!physical_index_oids)
	{
		UnlockRelationOid(logical_index_oid, AccessShareLock);
		GRN_LOG(ctx,
				GRN_LOG_WARNING,
				"%s <%s> has not children",
				tag,
				text_to_cstring(logical_index_name_text));
		PG_RETURN_ARRAYTYPE_P(construct_empty_array(TEXTOID));
	}
	ListCell *cell;

	Oid physical_index_oid;
	List *physical_table_names = NIL;
	char table_name_buffer[GRN_TABLE_MAX_KEY_SIZE];
	foreach (cell, physical_index_oids)
	{
		physical_index_oid = lfirst_oid(cell);
		if (logical_index_oid == physical_index_oid)
		{
			/**
			 * find_inheritance_children() returns contain parent index oid.
			 * Therefore, the first element of physical_index_oids skip.
			 */
			continue;
		}
		PGrnGetSourcesTableNameFromOid(physical_index_oid, table_name_buffer);
		physical_table_names =
			lappend(physical_table_names,
					(void *) (cstring_to_text(table_name_buffer)));
	}
	UnlockRelationOid(logical_index_oid, AccessShareLock);

	unsigned int n_elements = list_length(physical_table_names);
	Datum *physical_table_names_datum = palloc(n_elements * sizeof(Datum));
	unsigned int i = 0;
	foreach (cell, physical_table_names)
	{
		physical_table_names_datum[i++] = (Datum) lfirst(cell);
	}

	int dims[1] = {n_elements};
	int lbs[1] = {1};
	PG_RETURN_ARRAYTYPE_P(construct_md_array(physical_table_names_datum,
											 NULL,
											 1,
											 dims,
											 lbs,
											 TEXTOID,
											 -1,
											 false,
											 TYPALIGN_INT));
}
