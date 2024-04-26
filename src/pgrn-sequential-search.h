#pragma once

#include "pgrn-condition.h"

#include <utils/resowner.h>

typedef enum
{
	PGRN_SEQUENTIAL_SEARCH_UNKNOWN,
	PGRN_SEQUENTIAL_SEARCH_MATCH_TERM,
	PGRN_SEQUENTIAL_SEARCH_EQUAL_TEXT,
	PGRN_SEQUENTIAL_SEARCH_PREFIX,
	PGRN_SEQUENTIAL_SEARCH_QUERY,
	PGRN_SEQUENTIAL_SEARCH_SCRIPT,
	PGRN_SEQUENTIAL_SEARCH_EQUAL_QUERY,
} PGrnSequentialSearchType;

void PGrnInitializeSequentialSearch(void);
void PGrnFinalizeSequentialSearch(void);
void PGrnReleaseSequentialSearch(ResourceReleasePhase phase,
								 bool isCommit,
								 bool isTopLevel,
								 void *arg);
void PGrnSequentialSearchSetTargetText(const char *target,
									   unsigned int targetSize);
void PGrnSequentialSearchSetTargetTexts(ArrayType *targets,
										PGrnCondition *condition);
void PGrnSequentialSearchSetMatchTerm(PGrnCondition *condition);
void PGrnSequentialSearchSetEqualText(PGrnCondition *condition);
void PGrnSequentialSearchSetPrefix(PGrnCondition *condition);
void PGrnSequentialSearchSetQuery(PGrnCondition *condition,
								  PGrnSequentialSearchType type);
void PGrnSequentialSearchSetScript(PGrnCondition *condition);
bool PGrnSequentialSearchExecute(void);
