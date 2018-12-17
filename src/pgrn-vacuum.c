#include "pgroonga.h"

#include "pgrn-compatible.h"

PGRN_FUNCTION_INFO_V1(pgroonga_vacuum);

/**
 * pgroonga_vacuum() : bool
 */
Datum
pgroonga_vacuum(PG_FUNCTION_ARGS)
{
	PGrnRemoveUnusedTables();
	PG_RETURN_BOOL(true);
}
