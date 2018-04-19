#pragma once

#include <postgres.h>
#include <access/htup.h>
#include <utils/array.h>

#include <groonga.h>

void
PGrnFullTextSearchConditionDeconstruct(HeapTupleHeader header,
									   text **query,
									   ArrayType **weights,
									   text **indexName,
									   grn_obj *isTargets);

void
PGrnFullTextSearchConditionWithScorersDeconstruct(HeapTupleHeader header,
												  text **query,
												  ArrayType **weights,
												  ArrayType **scorers,
												  text **indexName,
												  grn_obj *isTargets);

