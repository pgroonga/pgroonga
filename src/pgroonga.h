#pragma once

#include <groonga.h>

#include <stdbool.h>

#include <postgres.h>

#include <access/skey.h>
#include <fmgr.h>
#include <utils/rel.h>

#include "pgrn-compatible.h"
#include "pgrn-constant.h"
#include "pgrn-search.h"

/* Used for pgroonga.so not other pgroonga_*.so. */
#define PGRN_MODULE_PGROONGA

/* Prefix for all log messages. */
#define PGRN_TAG "pgroonga"

/* Groonga strategy types */
#define PGrnLessStrategyNumber 1         /* operator < */
#define PGrnLessEqualStrategyNumber 2    /* operator <= */
#define PGrnEqualStrategyNumber 3        /* operator = */
#define PGrnGreaterEqualStrategyNumber 4 /* operator >= */
#define PGrnGreaterStrategyNumber 5      /* operator > */
#define PGrnLikeStrategyNumber 6         /* operator ~~ (LIKE) */
#define PGrnILikeStrategyNumber 7        /* operator ~~* (ILIKE) */
#define PGrnMatchStrategyNumber 8        /* operator %% (@ in Groonga) */
#define PGrnQueryStrategyNumber 9        /* operator @@ (Groonga query) */
#define PGrnRegexpStrategyNumber 10      /* operator @~ (@~ in Groonga)  */
#define PGrnContainStrategyNumber 11     /* operator @> */

#define PGrnMatchStrategyV2Number 12 /* operator &@ (@ in Groonga) */
/* operator &? (query in Groonga). Deprecated since 1.2.2. */
#define PGrnQueryStrategyV2DeprecatedNumber 13
/* operator &~? (similar search). Deprecated since 1.2.2. */
#define PGrnSimilarStrategyV2DeprecatedNumber 14
#define PGrnScriptStrategyV2Number 15   /* operator &`  (script in Groonga) */
#define PGrnPrefixStrategyV2Number 16   /* operator &^  (prefix search) */
#define PGrnPrefixRKStrategyV2Number 17 /* operator &^~ (prefix RK search) */
#define PGrnMatchInStrategyV2Number                                            \
	18 /* operator &@| (multiple conditions of @ in Groonga) */
/* operator &?| (multiple conditions of query in Groonga). Deprecated
 * since 1.2.2. */
#define PGrnQueryInStrategyV2Deprecated2Number 19
#define PGrnPrefixInStrategyV2Number                                           \
	20 /* operator &^| (multiple conditions of prefix search) */
#define PGrnPrefixRKInStrategyV2Number                                         \
	21 /* operator &^~| (multiple conditions of prefix RK search) */
#define PGrnRegexpStrategyV2Number 22 /* operator &~ (@~ in Groonga) */
#define PGrnContainStrategyV2Number                                            \
	23 /* operator &> (@ for vector in Groonga) */
/* operator &^> (prefix search against text[]). Deprecated since 1.2.1. */
#define PGrnPrefixStrategyV2DeprecatedNumber 24
/* operator &^~> (prefix RK search against text[]). Deprecated since 1.2.1. */
#define PGrnPrefixRKStrategyV2DeprecatedNumber 25
/* operator &@> (multiple conditions of @ in Groonga). Deprecated since 1.2.1.
 */
#define PGrnMatchInStrategyV2DeprecatedNumber 26
/* operator &?> (multiple conditions of query in Groonga). Deprecated
 * since 1.2.1. */
#define PGrnQueryInStrategyV2DeprecatedNumber 27
#define PGrnQueryStrategyV2Number 28   /* operator &@~  (query in Groonga) */
#define PGrnSimilarStrategyV2Number 29 /* operator &@* (similar search) */
#define PGrnQueryInStrategyV2Number                                            \
	30 /* operator &@~| (multiple conditions of query in Groonga) */
/* operator &@ with pgroonga_full_text_search_condition. Deprecated since 3.1.6.
 */
#define PGrnMatchFTSConditionStrategyV2Number 31
/* operator &@~ with pgroonga_full_text_search_condition. Deprecated
 * since 3.1.6. */
#define PGrnQueryFTSConditionStrategyV2Number 32
/* operator &@ with pgroonga_full_text_search_condition_with_scorers. Deprecated
 * since 3.1.6. */
#define PGrnMatchFTSConditionWithScorersStrategyV2Number 33
/* operator &@~ with pgroonga_full_text_search_condition_with_scorers.
 * Deprecated since 3.1.6. */
#define PGrnQueryFTSConditionWithScorersStrategyV2Number 34
/* operator &~| (multiple conditions of @~ in Groonga) */
#define PGrnRegexpInStrategyV2Number 35
/* operator !&^| (multiple conditions of not prefix search) */
#define PGrnNotPrefixInStrategyV2Number 36
/* operator &^ with pgroonga_full_text_search_condition. Deprecated since 3.1.6.
 */
#define PGrnPrefixFTSConditionStrategyV2Number 37
/* operator &= */
#define PGrnEqualStrategyV2Number 38
/* operator &= with pgroonga_full_text_search_condition. Deprecated since 3.1.6.
 */
#define PGrnEqualFTSConditionStrategyV2Number 39
/* operator &=~ */
#define PGrnEqualQueryStrategyV2Number 40
/* operator &=~ with pgroonga_full_text_search_condition. Deprecated
 * since 3.1.6. */
#define PGrnEqualQueryFTSConditionStrategyV2Number 41
/* operator &@ with pgroonga_condition. */
#define PGrnMatchConditionStrategyV2Number 42
/* operator &@~ with pgroonga_condition. */
#define PGrnQueryConditionStrategyV2Number 43
/* operator &^ with pgroonga_condition. */
#define PGrnPrefixConditionStrategyV2Number 44
/* operator &= with pgroonga_condition. */
#define PGrnEqualConditionStrategyV2Number 45
/* operator &=~ with pgroonga_condition. */
#define PGrnEqualQueryConditionStrategyV2Number 46
/* operator &~ with pgroonga_condition. */
#define PGrnRegexpConditionStrategyV2Number 47

#define PGRN_N_STRATEGIES PGrnRegexpConditionStrategyV2Number

extern grn_ctx PGrnContext;
static grn_ctx *ctx = &PGrnContext;
extern grn_obj PGrnInspectBuffer;

extern bool PGrnGroongaInitialized;
extern bool PGrnEnableParallelBuildCopy;
void PGrnEnsureDatabase(void);
void PGrnRemoveUnusedTables(void);
bool PGrnIndexIsPGroonga(Relation index);
Datum PGrnConvertToDatum(grn_obj *value, Oid typeID);

void
PGrnSearchBuildCondition(Relation index, ScanKey key, PGrnSearchData *data);
void
PGrnSearchDataInit(PGrnSearchData *data, Relation index, grn_obj *sourcesTable);
void PGrnSearchDataFree(PGrnSearchData *data);
