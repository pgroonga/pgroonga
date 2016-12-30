#pragma once

#include <postgres.h>

#include <groonga.h>

#include <utils/array.h>

void PGrnInitializeKeywords(void);
void PGrnFinalizeKeywords(void);

void PGrnKeywordsUpdateTable(ArrayType *keywords, grn_obj *keywordsTable);
