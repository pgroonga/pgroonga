#pragma once

#include <postgres.h>

#include <groonga.h>

typedef struct PGrnSearchData
{
	Relation index;
	grn_obj *sourcesTable;
	grn_obj targetColumns;
	grn_obj matchTargets;
	grn_obj sectionID;
	grn_obj *expression;
	grn_obj *expressionVariable;
	bool    isEmptyCondition;
	size_t  nExpressions;
} PGrnSearchData;

void PGrnSearchBuildConditionQuery(PGrnSearchData *data,
								   grn_obj *targetColumn,
								   const char *query,
								   unsigned int querySize);

void PGrnSearchBuildConditionBinaryOperation(PGrnSearchData *data,
											 grn_obj *targetColumn,
											 grn_obj *value,
											 grn_operator operator);
