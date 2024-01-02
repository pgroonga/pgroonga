#pragma once

#include <postgres.h>
#include <storage/itemptr.h>
#include <utils/relcache.h>

bool PGrnCtidIsAlive(Relation table, ItemPointer ctid);
uint64_t PGrnCtidPack(ItemPointer ctid);
ItemPointerData PGrnCtidUnpack(uint64_t packedCtid);
