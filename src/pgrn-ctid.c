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
	ItemPointerData realCtid;
	bool found;

	snapshot = GetActiveSnapshot();
	realCtid = *ctid;
	found = heap_hot_search(&realCtid, table, snapshot, NULL);

	return found;
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

