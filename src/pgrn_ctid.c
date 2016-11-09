#include "pgroonga.h"

#include "pgrn_ctid.h"

#include <access/heapam.h>
#include <access/htup.h>
#include <storage/buf.h>
#include <storage/bufmgr.h>
#include <utils/snapmgr.h>
#include <utils/snapshot.h>

bool
PGrnCtidIsAlive(Relation table, ItemPointer ctid)
{
	Buffer buffer;
	HeapTupleData tuple;
	Snapshot snapshot;
	ItemPointerData realCtid;
	bool allDead;
	bool found;
	bool isAlive = false;

	buffer = ReadBuffer(table, ItemPointerGetBlockNumber(ctid));
	snapshot = RegisterSnapshot(GetLatestSnapshot());
	realCtid = *ctid;
	found = heap_hot_search_buffer(&realCtid, table, buffer, snapshot, &tuple,
								   &allDead, true);
	if (found) {
		uint64 packedTupleCtid;

		packedTupleCtid = PGrnCtidPack(&(tuple.t_self));
		isAlive = (packedTupleCtid == PGrnCtidPack(ctid) ||
				   packedTupleCtid == PGrnCtidPack(&realCtid));
	}
	UnregisterSnapshot(snapshot);
	ReleaseBuffer(buffer);

	return isAlive;
}

uint64
PGrnCtidPack(ItemPointer ctid)
{
	BlockNumber blockNumber;
	OffsetNumber offsetNumber;

	blockNumber = ItemPointerGetBlockNumber(ctid);
	offsetNumber = ItemPointerGetOffsetNumber(ctid);
	return (((uint64)blockNumber << 16) | ((uint64)offsetNumber));
}

ItemPointerData
PGrnCtidUnpack(uint64 packedCtid)
{
	ItemPointerData	ctid;
	ItemPointerSet(&ctid,
				   (packedCtid >> 16) & 0xFFFFFFFF,
				   packedCtid & 0xFFFF);
	return ctid;
}

