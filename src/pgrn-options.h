#pragma once

#include <groonga.h>

#include <postgres.h>
#include <utils/rel.h>

typedef enum {
	PGRN_OPTION_USE_CASE_UNKNOWN,
	PGRN_OPTION_USE_CASE_FULL_TEXT_SEARCH,
	PGRN_OPTION_USE_CASE_REGEXP_SEARCH,
	PGRN_OPTION_USE_CASE_PREFIX_SEARCH
} PGrnOptionUseCase;

void PGrnInitializeOptions(void);
void PGrnFinalizeOptions(void);

void PGrnApplyOptionValues(Relation index,
						   int i,
						   PGrnOptionUseCase useCase,
						   grn_obj **tokenizer,
						   const char *defaultTokenizerName,
						   grn_obj **normalizer,
						   const char *defaultNormalizerName,
						   grn_obj **tokenFilters,
						   grn_table_flags *lexiconType,
						   grn_column_flags *indexFlags);

grn_expr_flags PGrnOptionsGetExprParseFlags(Relation index);

bytea *pgroonga_options(Datum reloptions,
						bool validate);
