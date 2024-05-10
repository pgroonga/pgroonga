#include "pgroonga.h"

#include "pgrn-trace-log.h"

PGDLLEXPORT PG_FUNCTION_INFO_V1(pgroonga_vacuum);

/**
 * pgroonga_vacuum() : bool
 */
Datum
pgroonga_vacuum(PG_FUNCTION_ARGS)
{
	PGRN_TRACE_LOG_ENTER();
	PGrnRemoveUnusedTables();
	PGRN_TRACE_LOG_EXIT();
	PG_RETURN_BOOL(true);
}
