#include "pgroonga.h"

#include "pgrn-compatible.h"
#include "pgrn-groonga.h"
#include "pgrn-jsonb.h"

#include <storage/lmgr.h>
#include <storage/lock.h>
#include <utils/builtins.h>

PGDLLEXPORT PG_FUNCTION_INFO_V1(pgroonga_flush);

/**
 * pgroonga.flush(indexName cstring) : boolean
 */
Datum
pgroonga_flush(PG_FUNCTION_ARGS)
{
	const char *tag = "[flush]";
	Datum indexNameDatum = PG_GETARG_DATUM(0);
	Datum indexOIDDatum;
	Oid indexOID;
	Relation index;

	indexOIDDatum = DirectFunctionCall1(regclassin, indexNameDatum);
	if (!OidIsValid(indexOIDDatum))
	{
		PGrnCheckRC(GRN_INVALID_ARGUMENT,
					"%s nonexistent index name: <%s>",
					tag,
					DatumGetCString(indexNameDatum));
	}
	indexOID = DatumGetObjectId(indexOIDDatum);

	LockRelationOid(indexOID, AccessShareLock);
	index = RelationIdGetRelation(indexOID);
	if (!RelationIsValid(index))
	{
		UnlockRelationOid(indexOID, AccessShareLock);
		PGrnCheckRC(GRN_INVALID_ARGUMENT,
					"%s failed to find index: <%s>",
					tag,
					DatumGetCString(indexNameDatum));
	}

	PG_TRY();
	{
		TupleDesc desc;
		unsigned int i;

		PGrnFlushObject(PGrnLookupSourcesTable(index, ERROR), true);

		desc = RelationGetDescr(index);
		for (i = 0; i < desc->natts; i++)
		{
			Form_pg_attribute attribute;

			attribute = TupleDescAttr(desc, i);
			if (PGrnAttributeIsJSONB(attribute->atttypid))
			{
				PGrnFlushObject(PGrnJSONBLookupValuesTable(index, i, ERROR),
								true);
				PGrnFlushObject(PGrnJSONBLookupPathsTable(index, i, ERROR),
								true);
				PGrnFlushObject(PGrnJSONBLookupTypesTable(index, i, ERROR),
								true);

				PGrnFlushObject(
					PGrnJSONBLookupFullTextSearchLexicon(index, i, ERROR),
					true);
				PGrnFlushObject(PGrnJSONBLookupStringLexicon(index, i, ERROR),
								true);
				PGrnFlushObject(PGrnJSONBLookupNumberLexicon(index, i, ERROR),
								true);
				PGrnFlushObject(PGrnJSONBLookupBooleanLexicon(index, i, ERROR),
								true);
				PGrnFlushObject(PGrnJSONBLookupSizeLexicon(index, i, ERROR),
								true);
			}
			else
			{
				PGrnFlushObject(PGrnLookupLexicon(index, i, ERROR), true);
			}
		}
		PGrnFlushObject(grn_ctx_db(ctx), false);
	}
	PG_CATCH();
	{
		RelationClose(index);
		UnlockRelationOid(indexOID, AccessShareLock);
		PG_RE_THROW();
	}
	PG_END_TRY();

	RelationClose(index);
	UnlockRelationOid(indexOID, AccessShareLock);

	PG_RETURN_BOOL(true);
}
