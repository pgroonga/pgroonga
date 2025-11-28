#pragma once

#include <groonga.h>

#include <postgres.h>
#include <utils/rel.h>

typedef enum
{
	PGRN_OPTION_USE_CASE_UNKNOWN,
	PGRN_OPTION_USE_CASE_FULL_TEXT_SEARCH,
	PGRN_OPTION_USE_CASE_REGEXP_SEARCH,
	PGRN_OPTION_USE_CASE_PREFIX_SEARCH,
	PGRN_OPTION_USE_CASE_SEMANTIC_SEARCH,
} PGrnOptionUseCase;

typedef struct PGrnResolvedOptions
{
	grn_obj *tokenizer;
	grn_obj *normalizers;
	grn_obj *tokenFilters;
	grn_obj *plugins;
	grn_id lexiconKeyTypeID;
	grn_table_flags lexiconType;
	grn_column_flags indexFlags;
	const char *modelName;
	bool needCentroidColumn;
	int32_t nGPULayers;
} PGrnResolvedOptions;

void PGrnInitializeOptions(void);
void PGrnFinalizeOptions(void);

void PGrnResolveOptionValues(Relation index,
							 int i,
							 PGrnOptionUseCase useCase,
							 const char *defaultTokenizer,
							 const char *defaultNormalizers,
							 PGrnResolvedOptions *resolvedOptions);

grn_expr_flags PGrnOptionsGetExprParseFlags(Relation index);

bytea *pgroonga_options(Datum reloptions, bool validate);
