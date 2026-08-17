#include "pgroonga.h"

#include "pgrn-physical-table-names.h"

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

/**
 * pgroonga_physical_table_name(indexName text, argument_prefix text) : array
 *
 */
Datum
pgroonga_physical_table_names(PG_FUNCTION_ARGS)
{
  PG_RETURN_ARRAYTYPE_P(construct_empty_array(TEXTOID));
}
