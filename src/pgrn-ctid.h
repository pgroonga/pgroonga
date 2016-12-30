#pragma once

#include <postgres.h>
#include <storage/itemptr.h>
#include <utils/relcache.h>

bool PGrnCtidIsAlive(Relation table, ItemPointer ctid);
uint64 PGrnCtidPack(ItemPointer ctid);
ItemPointerData PGrnCtidUnpack(uint64 packedCtid);
