#pragma once

#include <postgres.h>

#include <groonga.h>

#include <utils/array.h>
#include <utils/resowner.h>

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
PGrnInitializeSequentialSearch(void);
void
PGrnFinalizeSequentialSearch(void);
void
PGrnReleaseSequentialSearch(ResourceReleasePhase phase,
							bool isCommit,
							bool isTopLevel,
							void *arg);
void
PGrnSequentialSearchSetTargetText(const char *target,
								  unsigned int targetSize);
void
PGrnSequentialSearchSetTargetTexts(ArrayType *targets,
								   grn_obj *isTargets);
void
PGrnSequentialSearchSetMatchTerm(const char *term,
								 unsigned int termSize,
								 const char *indexName,
								 unsigned int indexNameSize);
void
PGrnSequentialSearchSetEqualText(const char *other,
								 unsigned int otherSize,
								 const char *indexName,
								 unsigned int indexNameSize);
void
PGrnSequentialSearchSetPrefix(const char *prefix,
							  unsigned int prefixSize,
							  const char *indexName,
							  unsigned int indexNameSize);
void
PGrnSequentialSearchSetQuery(const char *query,
							 unsigned int querySize,
							 const char *indexName,
							 unsigned int indexNameSize,
							 PGrnSequentialSearchType type);
void
PGrnSequentialSearchSetScript(const char *script,
							  unsigned int scriptSize,
							  const char *indexName,
							  unsigned int indexNameSize);
bool
PGrnSequentialSearchExecute(void);
