#pragma once

#include <postgres.h>

#include <access/heapam.h>
#include <access/htup_details.h>
#ifdef PGRN_SUPPORT_TABLEAM
#	include <access/tableam.h>
#endif
#include <catalog/pg_tablespace.h>

#include "pgrn-compatible.h"

typedef struct {
	LOCKMODE lockMode;
	Relation tablespaces;
	PGrnTableScanDesc scan;
} PGrnTablespaceIterator;

static inline void
PGrnTablespaceIteratorInitialize(PGrnTablespaceIterator *iterator,
								 LOCKMODE lockMode)
{
	iterator->lockMode = lockMode;
	iterator->tablespaces = pgrn_table_open(TableSpaceRelationId, lockMode);
	iterator->scan = pgrn_table_beginscan_catalog(iterator->tablespaces,
												  0,
												  NULL);
}

static inline Oid
PGrnTablespaceIteratorNext(PGrnTablespaceIterator *iterator)
{
	HeapTuple tuple;

	tuple = heap_getnext(iterator->scan, ForwardScanDirection);
	if (!HeapTupleIsValid(tuple))
		return InvalidOid;

#ifdef PGRN_SUPPORT_TABLEAM
	{
		Form_pg_tablespace form = (Form_pg_tablespace) GETSTRUCT(tuple);
		return form->oid;
	}
#else
	return HeapTupleGetOid(tuple);
#endif
}

static inline void
PGrnTablespaceIteratorFinalize(PGrnTablespaceIterator *iterator)
{
	heap_endscan(iterator->scan);
	pgrn_table_close(iterator->tablespaces, iterator->lockMode);
}

