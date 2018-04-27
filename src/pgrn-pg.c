#include "pgroonga.h"

#include "pgrn-compatible.h"

#include "pgrn-pg.h"

#include <access/heapam.h>
#include <access/htup_details.h>
#include <catalog/pg_tablespace.h>
#include <pgtime.h>
#include <storage/lmgr.h>
#include <utils/datetime.h>
#include <utils/rel.h>
#ifdef PGRN_SUPPORT_FILE_NODE_ID_TO_RELATION_ID
#	include <utils/relfilenodemap.h>
#endif

Relation
PGrnPGResolveFileNodeID(Oid fileNodeID,
						Oid *relationID,
						LOCKMODE lockMode)
{
#ifdef PGRN_SUPPORT_FILE_NODE_ID_TO_RELATION_ID
	Relation tableSpaces;
	HeapScanDesc scan;
	Relation relation = InvalidRelation;

	tableSpaces = heap_open(TableSpaceRelationId, AccessShareLock);
	scan = heap_beginscan_catalog(tableSpaces, 0, NULL);
	while (true)
	{
		HeapTuple tuple;

		tuple = heap_getnext(scan, ForwardScanDirection);

		if (!HeapTupleIsValid(tuple))
			break;

		*relationID = RelidByRelfilenode(HeapTupleGetOid(tuple),
										 fileNodeID);
		if (!OidIsValid(*relationID))
			continue;

		LockRelationOid(*relationID, lockMode);
		relation = RelationIdGetRelation(*relationID);
		if (RelationIsValid(relation))
			break;
		UnlockRelationOid(*relationID, lockMode);
	}
	heap_endscan(scan);
	heap_close(tableSpaces, AccessShareLock);

	return relation;
#else
	return InvalidRelation;
#endif
}

bool
PGrnPGIsValidFileNodeID(Oid fileNodeID)
{
#ifdef PGRN_SUPPORT_FILE_NODE_ID_TO_RELATION_ID
	Relation relation;
	Oid relationID = InvalidOid;
	LOCKMODE lockMode = AccessShareLock;
	bool valid;

	relation = PGrnPGResolveFileNodeID(fileNodeID, &relationID, lockMode);
	valid = RelationIsValid(relation);
	if (valid)
	{
		RelationClose(relation);
		UnlockRelationOid(relationID, lockMode);
	}

	return valid;
#else
	return true;
#endif
}

int
PGrnPGGetSessionTimezoneOffset(void)
{
	struct pg_tm tm;
	fsec_t fsec;
	int timezoneOffset = 0;
	GetCurrentTimeUsec(&tm, &fsec, &timezoneOffset);
	return timezoneOffset;
}

pg_time_t
PGrnPGTimestampToLocalTime(Timestamp timestamp)
{
	struct pg_tm tm;
	fsec_t fsec;
	int offset = 0;

	if (timestamp2tm(timestamp,
					 &offset,
					 &tm,
					 &fsec,
					 NULL,
					 NULL) != 0)
	{
		offset = PGrnPGGetSessionTimezoneOffset();
	}

	return timestamptz_to_time_t(timestamp) + offset;
}

Timestamp
PGrnPGLocalTimeToTimestamp(pg_time_t unixTimeLocal)
{
	pg_time_t unixTimeUTC;
	int sessionOffset;
	struct pg_tm tm;
	fsec_t fsec;
	int offset;
	Timestamp timestampMayDifferentSummaryTime;

	sessionOffset = PGrnPGGetSessionTimezoneOffset();
	timestampMayDifferentSummaryTime =
		time_t_to_timestamptz(unixTimeLocal + sessionOffset);
	/* TODO: This logic will be broken 1 hour around summer time change point. */
	if (timestamp2tm(timestampMayDifferentSummaryTime,
					 &offset,
					 &tm,
					 &fsec,
					 NULL,
					 NULL) != 0)
	{
		offset = sessionOffset;
	}
	unixTimeUTC = unixTimeLocal - offset;

	return time_t_to_timestamptz(unixTimeUTC);
}
