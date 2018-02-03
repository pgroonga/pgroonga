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

long int
PGrnPGGetSessionTimezoneOffset(void)
{
/* #ifdef WIN32 */
	struct pg_tm tm;
	fsec_t fsec;
	int timezoneOffset = 0;
	GetCurrentTimeUsec(&tm, &fsec, &timezoneOffset);
/* #else */
/* 	long int timezoneOffset = 0; */
/* 	pg_get_timezone_offset(session_timezone, &timezoneOffset); */
/* #endif */
	return timezoneOffset;
}
