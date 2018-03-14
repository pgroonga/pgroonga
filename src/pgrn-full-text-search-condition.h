#pragma once

#include <postgres.h>
#include <access/htup.h>
#include <utils/array.h>

#include <groonga.h>

#define PGRN_FULL_TEXT_SEARCH_CONDITION_QUERY_INDEX 0
#define PGRN_FULL_TEXT_SEARCH_CONDITION_WEIGHTS_INDEX 1
#define PGRN_FULL_TEXT_SEARCH_CONDITION_INDEX_NAME_INDEX 2

void
PGrnFullTextSearchConditionDeconstruct(HeapTupleHeader header,
									   text **query,
									   ArrayType **weights,
									   text **indexName,
									   grn_obj *isTargets);


