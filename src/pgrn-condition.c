#include "pgroonga.h"

#include "pgrn-condition.h"
#include "pgrn-pg.h"

#include <access/htup_details.h>
#include <utils/array.h>
#include <utils/builtins.h>
#include <utils/typcache.h>

void
PGrnConditionDeconstruct(PGrnCondition *condition, HeapTupleHeader header)
{
	Oid type;
	int32 typmod;
	TupleDesc desc;
	HeapTupleData tuple;
	char *rawData;
	long offset = 0;
	int i;
	bool mayFullIndexName = false;
	int queryIndex = -1;
	int weightsIndex = -1;
	int scorersIndex = -1;
	int schemaNameIndex = -1;
	int indexNameIndex = -1;
	int columnNameIndex = -1;
	int fuzzyMaxDistanceRatioIndex = -1;

	type = HeapTupleHeaderGetTypeId(header);
	typmod = HeapTupleHeaderGetTypMod(header);
	desc = lookup_rowtype_tupdesc(type, typmod);

	tuple.t_len = HeapTupleHeaderGetDatumLength(header);
	ItemPointerSetInvalid(&(tuple.t_self));
	tuple.t_tableOid = InvalidOid;
	tuple.t_data = header;

	rawData = ((char *) header) + header->t_hoff;

	if (desc->natts == 3)
	{
		/* pgroonga_full_text_search_condition */
		mayFullIndexName = true;
		queryIndex = 0;
		weightsIndex = 1;
		indexNameIndex = 2;
	}
	else if (desc->natts == 4)
	{
		/* pgroonga_full_text_search_condition_with_scorers */
		mayFullIndexName = true;
		queryIndex = 0;
		weightsIndex = 1;
		scorersIndex = 2;
		indexNameIndex = 3;
	}
	else
	{
		/* pgroonga_condition */
		queryIndex = 0;
		weightsIndex = 1;
		scorersIndex = 2;
		schemaNameIndex = 3;
		indexNameIndex = 4;
		columnNameIndex = 5;
		fuzzyMaxDistanceRatioIndex = 6;
	}

	for (i = 0; i < desc->natts; i++)
	{
		Form_pg_attribute attribute = TupleDescAttr(desc, i);
		bool isNULL;
		Datum datum;

		isNULL = (HeapTupleHasNulls(&tuple) && att_isnull(i, header->t_bits));

		if (isNULL)
		{
			if (i == queryIndex)
			{
				condition->query = NULL;
			}
			else if (i == weightsIndex)
			{
				condition->weights = NULL;
			}
			else if (i == scorersIndex)
			{
				condition->scorers = NULL;
			}
			else if (i == schemaNameIndex)
			{
				condition->schemaName = NULL;
			}
			else if (i == indexNameIndex)
			{
				condition->indexName = NULL;
			}
			else if (i == columnNameIndex)
			{
				condition->columnName = NULL;
			}
			else if (i == fuzzyMaxDistanceRatioIndex)
			{
				condition->fuzzyMaxDistanceRatio = -1.0;
			}
			continue;
		}

		offset = att_align_pointer(
			offset, attribute->attalign, -1, rawData + offset);
		datum = fetchatt(attribute, rawData + offset);

		if (i == queryIndex)
		{
			condition->query = DatumGetTextPP(datum);
		}
		else if (i == weightsIndex)
		{
			condition->weights = DatumGetArrayTypeP(datum);
		}
		else if (i == scorersIndex)
		{
			condition->scorers = DatumGetArrayTypeP(datum);
		}
		else if (i == schemaNameIndex)
		{
			condition->schemaName = DatumGetTextPP(datum);
		}
		else if (i == indexNameIndex)
		{
			condition->indexName = DatumGetTextPP(datum);
		}
		else if (i == columnNameIndex)
		{
			condition->columnName = DatumGetTextPP(datum);
		}
		else if (i == fuzzyMaxDistanceRatioIndex)
		{
			condition->fuzzyMaxDistanceRatio = DatumGetFloat4(datum);
		}

		offset =
			att_addlength_pointer(offset, attribute->attlen, rawData + offset);
	}

	ReleaseTupleDesc(desc);

	if (mayFullIndexName && condition->indexName)
	{
		const char *fullIndexName = VARDATA_ANY(condition->indexName);
		size_t fullIndexNameSize = VARSIZE_ANY_EXHDR(condition->indexName);
		const char *indexName;
		size_t indexNameSize;
		const char *columnName;
		size_t columnNameSize;
		PGrnPGFullIndexNameSplit(fullIndexName,
								 fullIndexNameSize,
								 &indexName,
								 &indexNameSize,
								 &columnName,
								 &columnNameSize);
		if (columnNameSize > 0)
		{
			condition->indexName =
				cstring_to_text_with_len(indexName, indexNameSize);
			condition->columnName =
				cstring_to_text_with_len(columnName, columnNameSize);
		}
	}

	if (condition->isTargets && condition->weights &&
		ARR_NDIM(condition->weights) == 1)
	{
		ArrayIterator iterator;
		Datum datum;
		bool isNULL;

		iterator = array_create_iterator(condition->weights, 0, NULL);
		while (array_iterate(iterator, &datum, &isNULL))
		{
			if (isNULL)
			{
				GRN_BOOL_PUT(ctx, condition->isTargets, true);
				continue;
			}

			GRN_BOOL_PUT(
				ctx, condition->isTargets, (DatumGetInt32(datum) != 0));
		}
		array_free_iterator(iterator);
	}
}
