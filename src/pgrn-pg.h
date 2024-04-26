#pragma once

#include "pgrn-compatible.h"

#include <storage/lock.h>
#include <utils/relcache.h>
#include <utils/timestamp.h>
#ifdef PGRN_HAVE_VARATT_H
#	include <varatt.h>
#endif

static inline bool
PGrnPGTextIsEmpty(text *text)
{
	if (!text)
		return true;
	if (VARSIZE_ANY_EXHDR(text) == 0)
		return true;
	return false;
}

void PGrnPGFullIndexNameSplit(const char *fullName,
							  size_t fullNameSize,
							  const char **indexName,
							  size_t *indexNameSize,
							  const char **attributeName,
							  size_t *attributeNameSize);
Oid PGrnPGIndexNameToID(const char *name);
Relation PGrnPGResolveIndexName(const char *name);
Relation PGrnPGResolveIndexID(Oid id);
const char *PGrnPGGetRelationNameByID(Oid id, char *buffer);
Oid PGrnPGIndexIDToFileNodeID(Oid indexID);
Relation
PGrnPGResolveFileNodeID(Oid fileNodeID, Oid *reationID, LOCKMODE lockMode);
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
int
PGrnPGResolveAttributeIndex(Relation index, const char *name, size_t nameSize);
