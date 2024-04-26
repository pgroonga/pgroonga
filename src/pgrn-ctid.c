#include "pgroonga.h"

#include "pgrn-ctid.h"

#include <access/heapam.h>
#include <access/htup.h>
#include <storage/buf.h>
#include <storage/bufmgr.h>
#include <utils/snapmgr.h>
#include <utils/snapshot.h>

bool
PGrnCtidIsAlive(Relation table, ItemPointer ctid)
{
	Snapshot snapshot;
	Buffer buffer;
	HeapTupleData heapTuple;
	bool found;

	snapshot = GetActiveSnapshot();
	buffer = ReadBuffer(table, ItemPointerGetBlockNumber(ctid));
	LockBuffer(buffer, BUFFER_LOCK_SHARE);
	/* table_index_fetch_tuple_check() may be better in the future. */
	found = heap_hot_search_buffer(
		ctid, table, buffer, snapshot, &heapTuple, NULL, true);
	LockBuffer(buffer, BUFFER_LOCK_UNLOCK);
	ReleaseBuffer(buffer);

	return found;
}

uint64_t
PGrnCtidPack(ItemPointer ctid)
{
	BlockNumber blockNumber;
	OffsetNumber offsetNumber;

	blockNumber = ItemPointerGetBlockNumber(ctid);
	offsetNumber = ItemPointerGetOffsetNumber(ctid);
	return (((uint64_t) blockNumber << 16) | ((uint64_t) offsetNumber));
}

ItemPointerData
PGrnCtidUnpack(uint64_t packedCtid)
{
	ItemPointerData ctid;
	ItemPointerSet(&ctid, (packedCtid >> 16) & 0xFFFFFFFF, packedCtid & 0xFFFF);
	return ctid;
}
