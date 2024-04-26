#pragma once

#include <postgres.h>

#include <access/htup.h>
#include <utils/array.h>

#include <groonga.h>

typedef struct
{
	text *query;
	ArrayType *weights;
	ArrayType *scorers;
	text *schemaName;
	text *indexName;
	text *columnName;
	grn_obj *isTargets;
} PGrnCondition;

void PGrnConditionDeconstruct(PGrnCondition *condition, HeapTupleHeader header);
