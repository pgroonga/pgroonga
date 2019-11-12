#include "pgroonga.h"

#include "pgrn-compatible.h"

#include "pgrn-pg.h"
#include "pgrn-tablespace.h"

#include <access/heapam.h>
#include <access/htup_details.h>
#ifdef PGRN_SUPPORT_TABLEAM
#	include <access/tableam.h>
#endif
#include <catalog/pg_type.h>
#include <pgtime.h>
#include <storage/lmgr.h>
#include <utils/builtins.h>
#include <utils/datetime.h>
#include <utils/rel.h>
#ifdef PGRN_SUPPORT_FILE_NODE_ID_TO_RELATION_ID
#	include <utils/relfilenodemap.h>
#endif
#include <utils/syscache.h>

Oid
PGrnPGIndexNameToID(const char *name)
{
	Datum indexIDDatum;
	Oid indexID;

	indexIDDatum = DirectFunctionCall1(regclassin, CStringGetDatum(name));
	indexID = DatumGetObjectId(indexIDDatum);
	if (!OidIsValid(indexID))
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_NAME),
				 errmsg("pgroonga: unknown index name: <%s>",
						name)));
	}

	return indexID;
}

Relation
PGrnPGResolveIndexName(const char *name)
{
	Oid indexID;
	indexID = PGrnPGIndexNameToID(name);
	return PGrnPGResolveIndexID(indexID);
}

Relation
PGrnPGResolveIndexID(Oid id)
{
	Relation index;

	index = RelationIdGetRelation(id);
	if (!RelationIsValid(index))
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_NAME),
				 errmsg("pgroonga: unknown index ID: <%u>",
						id)));
	}

	return index;
}

const char *
PGrnPGGetRelationNameByID(Oid id, char *buffer)
{
	Relation relation;

	relation = RelationIdGetRelation(id);
	if (!RelationIsValid(relation))
	{
		snprintf(buffer, NAMEDATALEN, "<invalid>(%u)", id);
		return buffer;
	}

	strncpy(buffer, relation->rd_rel->relname.data, NAMEDATALEN);
	RelationClose(relation);

	return buffer;
}

Oid
PGrnPGIndexIDToFileNodeID(Oid indexID)
{
	Oid fileNodeID;
	HeapTuple tuple;
	Form_pg_class indexClass;

	tuple = SearchSysCache1(RELOID, indexID);
	if (!HeapTupleIsValid(tuple))
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_NAME),
				 errmsg("pgroonga: "
						"failed to find file node ID for index: <%u>",
						indexID)));
	}

	indexClass = (Form_pg_class) GETSTRUCT(tuple);
	fileNodeID = indexClass->relfilenode;
	ReleaseSysCache(tuple);

	return fileNodeID;
}

Relation
PGrnPGResolveFileNodeID(Oid fileNodeID,
						Oid *relationID,
						LOCKMODE lockMode)
{
#ifdef PGRN_SUPPORT_FILE_NODE_ID_TO_RELATION_ID
	PGrnTablespaceIterator iterator;
	Relation relation = InvalidRelation;

	PGrnTablespaceIteratorInitialize(&iterator, AccessShareLock);
	while (true)
	{
		Oid tablespaceOid = PGrnTablespaceIteratorNext(&iterator);

		if (!OidIsValid(tablespaceOid))
			break;

		*relationID = RelidByRelfilenode(tablespaceOid, fileNodeID);
		if (!OidIsValid(*relationID))
			continue;

		LockRelationOid(*relationID, lockMode);
		relation = RelationIdGetRelation(*relationID);
		if (RelationIsValid(relation))
			break;
		UnlockRelationOid(*relationID, lockMode);
	}
	PGrnTablespaceIteratorFinalize(&iterator);

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

void
PGrnPGDatumExtractString(Datum datum,
						 Oid type,
						 const char **string,
						 unsigned int *size)
{
	switch (type)
	{
	case VARCHAROID:
	{
		VarChar *varCharData = DatumGetVarCharPP(datum);
		*string = VARDATA_ANY(varCharData);
		*size = VARSIZE_ANY_EXHDR(varCharData);
		break;
	}
	case TEXTOID:
	{
		text *textData = DatumGetTextPP(datum);
		*string = VARDATA_ANY(textData);
		*size = VARSIZE_ANY_EXHDR(textData);
		break;
	}
	default:
		break;
	}
}
