#include "pgroonga.h"

#include "pgrn-compatible.h"
#include "pgrn-database.h"

#include <access/heapam.h>
#include <access/htup_details.h>
#include <catalog/pg_tablespace.h>
#include <miscadmin.h>

PG_MODULE_MAGIC;

extern PGDLLEXPORT void _PG_init(void);

PGRN_FUNCTION_INFO_V1(pgroonga_database_remove);

/**
 * pgroonga_database_remove() : bool
 */
Datum
pgroonga_database_remove(PG_FUNCTION_ARGS)
{
	const LOCKMODE lock = RowExclusiveLock;
	Relation tablespaces;
	HeapScanDesc scan;

	tablespaces = heap_open(TableSpaceRelationId, lock);
	scan = heap_beginscan_catalog(tablespaces, 0, NULL);
	while (true)
	{
		HeapTuple tuple;
		Oid tablespace;
		char *databaseDirectoryPath;

		tuple = heap_getnext(scan, ForwardScanDirection);
		if (!HeapTupleIsValid(tuple))
			break;

		tablespace = HeapTupleGetOid(tuple);

		if (!pg_tablespace_ownercheck(tablespace, GetUserId()))
			break;

		databaseDirectoryPath = GetDatabasePath(MyDatabaseId, tablespace);
		PGrnDatabaseRemoveAllRelatedFiles(databaseDirectoryPath);
		pfree(databaseDirectoryPath);
	}
	heap_endscan(scan);
	heap_close(tablespaces, lock);

	PG_RETURN_BOOL(true);
}

void
_PG_init(void)
{
}
