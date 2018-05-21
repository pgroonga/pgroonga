#pragma once

#include <postgres.h>

#include <groonga.h>

#include <utils/array.h>

typedef enum  {
	PGRN_SEQUENTIAL_SEARCH_UNKNOWN,
	PGRN_SEQUENTIAL_SEARCH_MATCH_TERM,
	PGRN_SEQUENTIAL_SEARCH_QUERY,
	PGRN_SEQUENTIAL_SEARCH_SCRIPT,
} PGrnSequentialSearchType;

typedef struct PGrnSequentialSearchData
{
	grn_obj *table;
	grn_obj *textColumn;
	grn_obj *textsColumn;
	grn_id recordID;
	Oid indexOID;
	grn_obj *lexicon;
	grn_obj *indexColumn;
	grn_obj *indexColumnSource;
	grn_obj *matched;
	PGrnSequentialSearchType type;
	uint64_t expressionHash;
	grn_obj *expression;
	grn_obj *variable;
	bool useIndex;
} PGrnSequentialSearchData;

void
PGrnSequentialSearchDataInitialize(PGrnSequentialSearchData *data);
void
PGrnSequentialSearchDataFinalize(PGrnSequentialSearchData *data);
void
PGrnSequentialSearchDataPrepareText(PGrnSequentialSearchData *data,
									const char *target,
									unsigned int targetSize,
									const char *indexName,
									unsigned int indexNameSize);
void
PGrnSequentialSearchDataPrepareTexts(PGrnSequentialSearchData *data,
									 ArrayType *targets,
									 grn_obj *isTargets,
									 const char *indexName,
									 unsigned int indexNameSize);
void
PGrnSequentialSearchDataSetMatchTerm(PGrnSequentialSearchData *data,
									 const char *term,
									 unsigned int termSize);
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
