#pragma once

#include <postgres.h>
#include <storage/lock.h>
#include <utils/relcache.h>
#include <utils/timestamp.h>

Oid PGrnPGIndexNameToID(const char *name);
Relation PGrnPGResolveIndexName(const char *name);
Relation PGrnPGResolveIndexID(Oid id);
Oid PGrnPGIndexIDToFileNodeID(Oid indexID);
Relation PGrnPGResolveFileNodeID(Oid fileNodeID,
								 Oid *reationID,
								 LOCKMODE lockMode);
bool PGrnPGIsValidFileNodeID(Oid fileNodeID);
int PGrnPGGetSessionTimezoneOffset(void);
pg_time_t PGrnPGTimestampToLocalTime(Timestamp timestamp);
Timestamp PGrnPGLocalTimeToTimestamp(pg_time_t unixTimeLocal);
