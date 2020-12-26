#pragma once

#include <postgres.h>

#include <groonga.h>

#include <utils/array.h>

void PGrnInitializeKeywords(void);
void PGrnFinalizeKeywords(void);

void PGrnKeywordsSetNormalizer(grn_obj *keywordsTable,
							   const char *indexName,
							   Oid *previousIndexID);
void PGrnKeywordsUpdateTable(ArrayType *keywords,
							 grn_obj *keywordsTable);
