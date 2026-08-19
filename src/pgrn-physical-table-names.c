#include "pgroonga.h"

#include "pgrn-groonga.h"
#include "pgrn-pg.h"
#include "pgrn-physical-table-names.h"

#include <utils/builtins.h>

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

/**
 * pgroonga_physical_table_names(logical_index_name text, argument_prefix text)
 * : text[]
 */
Datum
pgroonga_physical_table_names(PG_FUNCTION_ARGS)
{
	const char *tag = "[physical-table-names]";
	text *logical_index_name_text = PG_GETARG_TEXT_PP(0);

	PGrnGetLogicalIndexOid(tag, logical_index_name_text);
	PG_RETURN_ARRAYTYPE_P(construct_empty_array(TEXTOID));
}
