#pragma once

#include <postgres.h>
#include <storage/lock.h>
#include <utils/relcache.h>

Relation PGrnPGResolveFileNodeID(Oid fileNodeID,
								 Oid *reationID,
								 LOCKMODE lockMode);
bool PGrnPGIsValidFileNodeID(Oid fileNodeID);
