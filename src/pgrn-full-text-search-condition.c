#include "pgroonga.h"

#include "pgrn-compatible.h"

#include "pgrn-full-text-search-condition.h"
#include "pgrn-global.h"

#include <access/htup_details.h>
#include <utils/array.h>
#include <utils/typcache.h>

static grn_ctx *ctx = &PGrnContext;

void
PGrnFullTextSearchConditionDeconstruct(HeapTupleHeader header,
									   text **query,
									   ArrayType **weights,
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
		Form_pg_attribute attribute = desc->attrs[i];
		bool isNULL;
		Datum datum;

		isNULL = (HeapTupleHasNulls(&tuple) && att_isnull(i, header->t_bits));

		if (isNULL)
		{
			switch (i)
			{
			case PGRN_FULL_TEXT_SEARCH_CONDITION_QUERY_INDEX:
				if (query)
					*query = NULL;
				break;
			case PGRN_FULL_TEXT_SEARCH_CONDITION_WEIGHTS_INDEX:
				if (weights)
					*weights = NULL;
				break;
			case PGRN_FULL_TEXT_SEARCH_CONDITION_INDEX_NAME_INDEX:
				if (indexName)
					*indexName = NULL;
				break;
			default:
				break;
			}
			continue;
		}

		offset = att_align_pointer(offset,
								   attribute->attalign,
								   -1,
								   rawData + offset);
		datum = fetchatt(attribute, rawData + offset);

		switch (i)
		{
		case PGRN_FULL_TEXT_SEARCH_CONDITION_QUERY_INDEX:
			if (query)
				*query = DatumGetTextPP(datum);
			break;
		case PGRN_FULL_TEXT_SEARCH_CONDITION_WEIGHTS_INDEX:
			weightsLocal = DatumGetArrayTypeP(datum);
			if (weights)
				*weights = weightsLocal;
			break;
		case PGRN_FULL_TEXT_SEARCH_CONDITION_INDEX_NAME_INDEX:
			if (indexName)
				*indexName = DatumGetTextPP(datum);
			break;
		default:
			break;
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
