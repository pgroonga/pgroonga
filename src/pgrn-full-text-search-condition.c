#include "pgroonga.h"

#include "pgrn-compatible.h"

#include "pgrn-full-text-search-condition.h"
#include "pgrn-global.h"

#include <access/htup_details.h>
#include <utils/array.h>
#include <utils/typcache.h>

static grn_ctx *ctx = &PGrnContext;

static void
PGrnFullTextSearchConditionDeconstructGeneric(HeapTupleHeader header,
											  int queryIndex,
											  text **query,
											  int weightsIndex,
											  ArrayType **weights,
											  int scorersIndex,
											  ArrayType **scorers,
											  int indexNameIndex,
											  text **indexName,
											  grn_obj *isTargets)
{
	Oid type;
	int32 typmod;
	TupleDesc desc;
	HeapTupleData tuple;
	char *rawData;
	long offset = 0;
	int i;
	ArrayType *weightsLocal = NULL;

	type = HeapTupleHeaderGetTypeId(header);
	typmod = HeapTupleHeaderGetTypMod(header);
	desc = lookup_rowtype_tupdesc(type, typmod);

	tuple.t_len = HeapTupleHeaderGetDatumLength(header);
	ItemPointerSetInvalid(&(tuple.t_self));
	tuple.t_tableOid = InvalidOid;
	tuple.t_data = header;

	rawData = (char *) header + header->t_hoff;

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
				if (query)
					*query = NULL;
			}
			else if (i == weightsIndex)
			{
				if (weights)
					*weights = NULL;
			}
			else if (i == scorersIndex)
			{
				if (scorers)
					*scorers = NULL;
			}
			else if (i == indexNameIndex)
			{
				if (indexName)
					*indexName = NULL;
			}
			continue;
		}

		offset = att_align_pointer(offset,
								   attribute->attalign,
								   -1,
								   rawData + offset);
		datum = fetchatt(attribute, rawData + offset);

		if (i == queryIndex)
		{
			if (query)
				*query = DatumGetTextPP(datum);
		}
		else if (i == weightsIndex)
		{
			weightsLocal = DatumGetArrayTypeP(datum);
			if (weights)
				*weights = weightsLocal;
		}
		else if (i == scorersIndex)
		{
			if (scorers)
				*scorers = DatumGetArrayTypeP(datum);
		}
		else if (i == indexNameIndex)
		{
			if (indexName)
				*indexName = DatumGetTextPP(datum);
		}

		offset = att_addlength_pointer(offset,
									   attribute->attlen,
									   rawData + offset);
	}

	ReleaseTupleDesc(desc);

	if (isTargets && weightsLocal && ARR_NDIM(weightsLocal) == 1)
	{
		ArrayIterator iterator;
		int i;
		Datum datum;
		bool isNULL;

		iterator = pgrn_array_create_iterator(weightsLocal, 0);
		for (i = 0; array_iterate(iterator, &datum, &isNULL); i++)
		{
			if (isNULL)
			{
				GRN_BOOL_PUT(ctx, isTargets, GRN_TRUE);
				continue;
			}

			GRN_BOOL_PUT(ctx, isTargets, (DatumGetInt32(datum) != 0));
		}
		array_free_iterator(iterator);
	}
}

void
PGrnFullTextSearchConditionDeconstruct(HeapTupleHeader header,
									   text **query,
									   ArrayType **weights,
									   text **indexName,
									   grn_obj *isTargets)
{
	PGrnFullTextSearchConditionDeconstructGeneric(header,
												  0, query,
												  1, weights,
												  -1, NULL,
												  2, indexName,
												  isTargets);
}

void
PGrnFullTextSearchConditionWithScorersDeconstruct(HeapTupleHeader header,
												  text **query,
												  ArrayType **weights,
												  ArrayType **scorers,
												  text **indexName,
												  grn_obj *isTargets)
{
	PGrnFullTextSearchConditionDeconstructGeneric(header,
												  0, query,
												  1, weights,
												  2, scorers,
												  3, indexName,
												  isTargets);
}
