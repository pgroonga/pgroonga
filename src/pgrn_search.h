#pragma once

#include <postgres.h>

#include <groonga.h>

typedef struct PGrnSearchData
{
	grn_obj *sourcesTable;
	grn_obj targetColumns;
	grn_obj matchTargets;
	grn_obj sectionID;
	grn_obj *expression;
	grn_obj *expressionVariable;
	bool    isEmptyCondition;
} PGrnSearchData;
