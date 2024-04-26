#pragma once

#include "pgrn-compatible.h"

#include <access/heapam.h>
#include <access/htup_details.h>
#include <access/tableam.h>
#include <catalog/pg_tablespace.h>

typedef struct
{
	LOCKMODE lockMode;
	Relation tablespaces;
	TableScanDesc scan;
} PGrnTablespaceIterator;

static inline void
PGrnTablespaceIteratorInitialize(PGrnTablespaceIterator *iterator,
								 LOCKMODE lockMode)
{
	iterator->lockMode = lockMode;
	iterator->tablespaces = table_open(TableSpaceRelationId, lockMode);
	iterator->scan = table_beginscan_catalog(iterator->tablespaces, 0, NULL);
}

static inline Oid
PGrnTablespaceIteratorNext(PGrnTablespaceIterator *iterator)
{
	HeapTuple tuple;

	tuple = heap_getnext(iterator->scan, ForwardScanDirection);
	if (!HeapTupleIsValid(tuple))
		return InvalidOid;

	{
		Form_pg_tablespace form = (Form_pg_tablespace) GETSTRUCT(tuple);
		return form->oid;
	}
}

static inline void
PGrnTablespaceIteratorFinalize(PGrnTablespaceIterator *iterator)
{
	heap_endscan(iterator->scan);
	table_close(iterator->tablespaces, iterator->lockMode);
}
