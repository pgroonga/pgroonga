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

typedef struct PGrnSequentialSearchData
{
	grn_obj *table;
	grn_obj *textColumn;
	grn_obj *textsColumn;
	grn_obj *targetColumn;
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
	grn_expr_flags exprFlags;
} PGrnSequentialSearchData;

void
PGrnSequentialSearchDataInitialize(PGrnSequentialSearchData *data);
void
PGrnSequentialSearchDataFinalize(PGrnSequentialSearchData *data);
void
PGrnSequentialSearchDataPrepareText(PGrnSequentialSearchData *data,
									const char *target,
									unsigned int targetSize);
void
PGrnSequentialSearchDataPrepareTexts(PGrnSequentialSearchData *data,
									 ArrayType *targets,
									 grn_obj *isTargets);
void
PGrnSequentialSearchDataSetMatchTerm(PGrnSequentialSearchData *data,
									 const char *term,
									 unsigned int termSize,
									 const char *indexName,
									 unsigned int indexNameSize);
void
PGrnSequentialSearchDataSetEqualText(PGrnSequentialSearchData *data,
									 const char *other,
									 unsigned int otherSize,
									 const char *indexName,
									 unsigned int indexNameSize);
void
PGrnSequentialSearchDataSetPrefix(PGrnSequentialSearchData *data,
								  const char *prefix,
								  unsigned int prefixSize,
								  const char *indexName,
								  unsigned int indexNameSize);
void
PGrnSequentialSearchDataSetQuery(PGrnSequentialSearchData *data,
								 const char *query,
								 unsigned int querySize,
								 const char *indexName,
								 unsigned int indexNameSize,
								 PGrnSequentialSearchType type);
void
PGrnSequentialSearchDataSetScript(PGrnSequentialSearchData *data,
								  const char *script,
								  unsigned int scriptSize,
								  const char *indexName,
								  unsigned int indexNameSize);
bool
PGrnSequentialSearchDataExecute(PGrnSequentialSearchData *data);
