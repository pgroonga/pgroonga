#pragma once

#include <postgres.h>
#include <storage/lockdefs.h>
#include <utils/relcache.h>

Relation PGrnPGResolveFileNodeID(Oid fileNodeID,
								 Oid *reationID,
								 LOCKMODE lockMode);
bool PGrnPGIsValidFileNodeID(Oid fileNodeID);
