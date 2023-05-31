#pragma once

#include <postgres.h>
#include <storage/lock.h>
#include <utils/relcache.h>
#include <utils/timestamp.h>

Oid PGrnPGIndexNameToID(const char *name);
Relation PGrnPGResolveIndexName(const char *name);
Relation PGrnPGResolveIndexID(Oid id);
const char *PGrnPGGetRelationNameByID(Oid id, char *buffer);
Oid PGrnPGIndexIDToFileNodeID(Oid indexID);
Relation PGrnPGResolveFileNodeID(Oid fileNodeID,
								 Oid *reationID,
								 LOCKMODE lockMode);
bool PGrnPGIsValidFileNodeID(Oid fileNodeID);
int PGrnPGGetSessionTimezoneOffset(void);
pg_time_t PGrnPGTimestampToLocalTime(Timestamp timestamp);
Timestamp PGrnPGLocalTimeToTimestamp(pg_time_t unixTimeLocal);
void PGrnPGDatumExtractString(Datum datum,
							  Oid type,
							  const char **string,
							  unsigned int *size);
bool PGrnPGHavePreparedTransaction(void);
bool PGrnPGIsParentIndex(Relation index);
