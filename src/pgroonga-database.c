#include "pgroonga.h"

#include "pgrn-compatible.h"
#include "pgrn-database.h"

#include <access/heapam.h>
#include <access/htup_details.h>
#ifdef PGRN_SUPPORT_TABLEAM
#	include <access/tableam.h>
#endif
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
	Relation tableSpaces;
	PGrnTableScanDesc scan;

	tableSpaces = heap_open(TableSpaceRelationId, lock);
	scan = pgrn_table_beginscan_catalog(tableSpaces, 0, NULL);
	while (true)
	{
		HeapTuple tuple;
		Form_pg_tablespace form;
		Oid tableSpace;
		char *databaseDirectoryPath;

		tuple = heap_getnext(scan, ForwardScanDirection);
		if (!HeapTupleIsValid(tuple))
			break;

		form = (Form_pg_tablespace) GETSTRUCT(tuple);
		tableSpace = form->oid;
		if (!pg_tablespace_ownercheck(tableSpace, GetUserId()))
			break;

		databaseDirectoryPath = GetDatabasePath(MyDatabaseId, tableSpace);
		PGrnDatabaseRemoveAllRelatedFiles(databaseDirectoryPath);
		pfree(databaseDirectoryPath);
	}
	heap_endscan(scan);
	heap_close(tableSpaces, lock);

	PG_RETURN_BOOL(true);
}

void
_PG_init(void)
{
}
