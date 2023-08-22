#pragma once

#include <postgres.h>

#include <groonga.h>

#include <utils/array.h>

typedef enum  {
	PGRN_SEQUENTIAL_SEARCH_UNKNOWN,
	PGRN_SEQUENTIAL_SEARCH_MATCH_TERM,
	PGRN_SEQUENTIAL_SEARCH_EQUAL_TEXT,
	PGRN_SEQUENTIAL_SEARCH_PREFIX,
	PGRN_SEQUENTIAL_SEARCH_QUERY,
	PGRN_SEQUENTIAL_SEARCH_SCRIPT,
	PGRN_SEQUENTIAL_SEARCH_EQUAL_QUERY,
} PGrnSequentialSearchType;

void
PGrnInitializeSequentialSearchData(void);
void
PGrnFinalizeSequentialSearchData(void);
void
PGrnSequentialSearchDataPrepareText(const char *target,
									unsigned int targetSize);
void
PGrnSequentialSearchDataPrepareTexts(ArrayType *targets,
									 grn_obj *isTargets);
void
PGrnSequentialSearchDataSetMatchTerm(const char *term,
									 unsigned int termSize,
									 const char *indexName,
									 unsigned int indexNameSize);
void
PGrnSequentialSearchDataSetEqualText(const char *other,
									 unsigned int otherSize,
									 const char *indexName,
									 unsigned int indexNameSize);
void
PGrnSequentialSearchDataSetPrefix(const char *prefix,
								  unsigned int prefixSize,
								  const char *indexName,
								  unsigned int indexNameSize);
void
PGrnSequentialSearchDataSetQuery(const char *query,
								 unsigned int querySize,
								 const char *indexName,
								 unsigned int indexNameSize,
								 PGrnSequentialSearchType type);
void
PGrnSequentialSearchDataSetScript(const char *script,
								  unsigned int scriptSize,
								  const char *indexName,
								  unsigned int indexNameSize);
bool
PGrnSequentialSearchDataExecute(void);
