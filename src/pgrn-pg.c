#include "pgroonga.h"

#include "pgrn-compatible.h"

#include "pgrn-groonga.h"
#include "pgrn-pg.h"
#include "pgrn-tablespace.h"

#include <access/heapam.h>
#include <access/htup_details.h>
#include <access/tableam.h>
#include <catalog/pg_type.h>
#include <executor/executor.h>
#include <pgtime.h>
#include <nodes/execnodes.h>
#include <storage/lmgr.h>
#include <utils/builtins.h>
#include <utils/datetime.h>
#include <utils/fmgroids.h>
#include <utils/rel.h>
#if PG_VERSION_NUM >= 160000
#	include <utils/relfilenumbermap.h>
#else
#	include <utils/relfilenodemap.h>
#endif
#include <utils/syscache.h>

#include <string.h>

void
PGrnPGFullIndexNameSplit(const char *fullName,
						 size_t fullNameSize,
						 const char **indexName,
						 size_t *indexNameSize,
						 const char **attributeName,
						 size_t *attributeNameSize)
{
	*indexName = NULL;
	*indexNameSize = 0;
	*attributeName = NULL;
	*attributeNameSize = 0;

	if (fullNameSize == 0)
	{
		return;
	}

	/* Should we use database encoding? */
	{
		const char *current = fullName;
		const char *end = fullName + fullNameSize;
		for (; current < end; current++)
		{
			if (current[0] == '.')
				break;
		}
		*indexName = fullName;
		*indexNameSize = current - fullName;
		if (current == end) {
			return;
		}
		/* +1/-1 is for '.' */
		*attributeName = current + 1;
		*attributeNameSize = end - current - 1;
	}
}

Oid
PGrnPGIndexNameToID(const char *name)
{
	Datum indexIDDatum;
	Oid indexID;

	indexIDDatum = DirectFunctionCall1(regclassin, CStringGetDatum(name));
	indexID = DatumGetObjectId(indexIDDatum);
	if (!OidIsValid(indexID))
	{
		PGrnCheckRC(GRN_INVALID_ARGUMENT,
					"unknown index name: <%s>",
					name);
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
		PGrnCheckRC(GRN_INVALID_ARGUMENT,
					"pgroonga: unknown index ID: <%u>",
					id);
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
		PGrnCheckRC(GRN_INVALID_ARGUMENT,
					"failed to find file node ID for index: <%u>",
					indexID);
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
	PGrnTablespaceIterator iterator;
	Relation relation = InvalidRelation;

	PGrnTablespaceIteratorInitialize(&iterator, AccessShareLock);
	while (true)
	{
		Oid tablespaceOid = PGrnTablespaceIteratorNext(&iterator);

		if (!OidIsValid(tablespaceOid))
			break;

		*relationID = PGrnRelidByRelfilenumber(tablespaceOid, fileNodeID);
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
}

bool
PGrnPGIsValidFileNodeID(Oid fileNodeID)
{
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

bool
PGrnPGHavePreparedTransaction(void)
{
	MemoryContext memoryContext;
	MemoryContext oldMemoryContext;
	FmgrInfo flinfo;
	ReturnSetInfo rsinfo;
	EState *estate = NULL;
	ExprContext *econtext;
	bool have = false;
	LOCAL_FCINFO(fcinfo, 0);

	memoryContext = AllocSetContextCreate(CurrentMemoryContext,
										  "PGrnPGHavePreparedTransaction",
										  ALLOCSET_SMALL_SIZES);
	oldMemoryContext = MemoryContextSwitchTo(memoryContext);
	PG_TRY();
	{
		estate = CreateExecutorState();
		econtext = CreateExprContext(estate);
		fmgr_info(F_PG_PREPARED_XACT, &flinfo);
		InitFunctionCallInfoData(*fcinfo,
								 &flinfo,
								 0,
								 InvalidOid,
								 NULL,
								 (fmNodePtr) &rsinfo);
		rsinfo.type = T_ReturnSetInfo;
		rsinfo.econtext = econtext;
		rsinfo.expectedDesc = NULL;
		rsinfo.allowedModes = SFRM_ValuePerCall;
		rsinfo.returnMode = SFRM_ValuePerCall;
		rsinfo.setResult = NULL;
		rsinfo.setDesc = NULL;
		rsinfo.isDone = ExprSingleResult;

		while (true) {
			flinfo.fn_addr(fcinfo);
			if (rsinfo.isDone == ExprEndResult) {
				break;
			}
			have = true;
		}
	}
	PG_CATCH();
	{
		if (estate)
			FreeExecutorState(estate);
		MemoryContextSwitchTo(oldMemoryContext);
		MemoryContextDelete(memoryContext);

		PG_RE_THROW();
	}
	PG_END_TRY();

	FreeExecutorState(estate);
	MemoryContextSwitchTo(oldMemoryContext);
	MemoryContextDelete(memoryContext);

	return have;
}

bool
PGrnPGIsParentIndex(Relation index)
{
	return PGRN_RELATION_GET_LOCATOR_NUMBER(index) == InvalidOid;
}

int
PGrnPGResolveAttributeIndex(Relation index,
							const char *name,
							size_t nameSize)
{
	int i;

	if (nameSize == 0)
	{
		return -1;
	}

	for (i = 0; i < index->rd_att->natts; i++)
	{
		const char *attributeName =
			TupleDescAttr(index->rd_att, i)->attname.data;
		if (strlen(attributeName) != nameSize)
		{
			continue;
		}
		if (memcmp(attributeName, name, nameSize) != 0)
		{
			continue;
		}
		return i;
	}

	return -1;
}
