#include "pgroonga.h"

#include "pgrn-groonga.h"
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
	return PGRN_RELKIND_HAS_PARTITIONS(RelationGetForm(relation)->relkind);
}

static Oid
PGrnGetLogicalIndexOid(const char *tag, text *logical_index_name_text)
{
	Datum logical_index_oid_datum = DirectFunctionCall1(
		regclassin, CStringGetDatum(text_to_cstring(logical_index_name_text)));
	if (!OidIsValid(logical_index_oid_datum))
	{
		PGrnCheckRC(GRN_INVALID_ARGUMENT,
					"%s nonexistent index name: <%s>",
					tag,
					text_to_cstring(logical_index_name_text));
	}

	Oid logical_index_oid = DatumGetObjectId(logical_index_oid_datum);
	Relation logical_index = RelationIdGetRelation(logical_index_oid);
	if (!PGrnRelationIsPartitionedIndex(logical_index))
	{
		RelationClose(logical_index);
		PGrnCheckRC(GRN_INVALID_ARGUMENT,
					"%s the specified index is not partitioned index: <%s>",
					tag,
					text_to_cstring(logical_index_name_text));
	}
	RelationClose(logical_index);

	return logical_index_oid;
}

/**
 * pgroonga_physical_table_names(logical_table_name text, argument_prefix text)
 * : text[]
 */
Datum
pgroonga_physical_table_names(PG_FUNCTION_ARGS)
{
	const char *tag = "[pyhsical-table-names]";
	text *logical_index_name_text = PG_GETARG_TEXT_PP(0);

	PGrnGetLogicalIndexOid(tag, logical_index_name_text);
	PG_RETURN_ARRAYTYPE_P(construct_empty_array(TEXTOID));
}
