#include "pgroonga.h"

#include "pgrn-compatible.h"
#include "pgrn-database.h"
#include "pgrn-tablespace.h"

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
	PGrnTablespaceIterator iterator;

	PGrnTablespaceIteratorInitialize(&iterator, RowExclusiveLock);
	while (true)
	{
		Oid tablespaceOid;
		char *databaseDirectoryPath;

		tablespaceOid = PGrnTablespaceIteratorNext(&iterator);
		if (!OidIsValid(tablespaceOid))
			break;

		if (!pg_tablespace_ownercheck(tablespaceOid, GetUserId()))
			break;

		databaseDirectoryPath = GetDatabasePath(MyDatabaseId, tablespaceOid);
		PGrnDatabaseRemoveAllRelatedFiles(databaseDirectoryPath);
		pfree(databaseDirectoryPath);
	}
	PGrnTablespaceIteratorFinalize(&iterator);

	PG_RETURN_BOOL(true);
}

void
_PG_init(void)
{
}
