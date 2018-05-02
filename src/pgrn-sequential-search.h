#pragma once

#include <postgres.h>

#include <groonga.h>

typedef enum  {
	PGRN_SEQUENTIAL_SEARCH_UNKNOWN,
	PGRN_SEQUENTIAL_SEARCH_QUERY,
	PGRN_SEQUENTIAL_SEARCH_SCRIPT,
} PGrnSequentialSearchType;

typedef struct PGrnSequentialSearchData
{
	grn_obj *table;
	grn_obj *textColumn;
	grn_id recordID;
	Oid indexOID;
	grn_obj *lexicon;
	grn_obj *indexColumn;
	grn_obj *matched;
	PGrnSequentialSearchType type;
	uint64_t expressionHash;
	grn_obj *expression;
} PGrnSequentialSearchData;

void
PGrnSequentialSearchDataInitialize(PGrnSequentialSearchData *data);
void
PGrnSequentialSearchDataFinalize(PGrnSequentialSearchData *data);
void
PGrnSequentialSearchDataPrepare(PGrnSequentialSearchData *data,
								const char *target,
								unsigned int targetSize,
								const char *indexName,
								unsigned int indexNameSize);
void
PGrnSequentialSearchDataSetQuery(PGrnSequentialSearchData *data,
								 const char *query,
								 unsigned int querySize);
void
PGrnSequentialSearchDataSetScript(PGrnSequentialSearchData *data,
								  const char *script,
								  unsigned int scriptSize);
bool
PGrnSequentialSearchDataExecute(PGrnSequentialSearchData *data);
