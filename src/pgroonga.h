#pragma once

#include <stdbool.h>

#include <postgres.h>

#include <fmgr.h>
#include <utils/rel.h>

/* Must not be the same value as one of error level codes in
 * PostgreSQL such as ERROR and WARNING. */
#define PGRN_ERROR_LEVEL_IGNORE 0

/* Default values */
#ifndef PGRN_DEFAULT_TOKENIZER
#	define PGRN_DEFAULT_TOKENIZER "TokenBigram"
#endif
#ifndef PGRN_DEFAULT_NORMALIZERS
#	define PGRN_DEFAULT_NORMALIZERS "NormalizerAuto"
#endif

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

#define PGrnMatchStrategyV2Number 12 /* operator &@	(@ in Groonga) */
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

#define PGRN_N_STRATEGIES PGrnEqualQueryConditionStrategyV2Number

/* file and table names */
#define PGrnLogPathDefault "pgroonga.log"
#define PGrnQueryLogPathDefault "none"
#define PGrnDatabaseBasename "pgrn"
#define PGrnBuildingSourcesTableNamePrefix "BuildingSources"
#define PGrnBuildingSourcesTableNamePrefixLength                               \
	(sizeof(PGrnBuildingSourcesTableNamePrefix) - 1)
#define PGrnBuildingSourcesTableNameFormat                                     \
	PGrnBuildingSourcesTableNamePrefix "%u"
#define PGrnSourcesTableNamePrefix "Sources"
#define PGrnSourcesTableNamePrefixLength                                       \
	(sizeof(PGrnSourcesTableNamePrefix) - 1)
#define PGrnSourcesTableNameFormat PGrnSourcesTableNamePrefix "%u"
#define PGrnSourcesCtidColumnName "ctid"
#define PGrnSourcesCtidColumnNameLength (sizeof(PGrnSourcesCtidColumnName) - 1)
#define PGrnJSONPathsTableNamePrefix "JSONPaths"
#define PGrnJSONPathsTableNameFormat PGrnJSONPathsTableNamePrefix "%u_%u"
#define PGrnJSONValuesTableNamePrefix "JSONValues"
#define PGrnJSONValuesTableNameFormat PGrnJSONValuesTableNamePrefix "%u_%u"
#define PGrnJSONTypesTableNamePrefix "JSONTypes"
#define PGrnJSONTypesTableNameFormat PGrnJSONTypesTableNamePrefix "%u_%u"
#define PGrnJSONValueLexiconNamePrefix "JSONValueLexicon"
#define PGrnJSONValueLexiconNameFormat PGrnJSONValueLexiconNamePrefix "%s%u_%u"
#define PGrnLexiconNamePrefix "Lexicon"
#define PGrnLexiconNameFormat PGrnLexiconNamePrefix "%u_%u"
#define PGrnIndexColumnName "index"
#define PGrnIndexColumnNameFormat PGrnLexiconNameFormat "." PGrnIndexColumnName

#define PGRN_EXPR_QUERY_PARSE_FLAGS                                            \
	(GRN_EXPR_SYNTAX_QUERY | GRN_EXPR_ALLOW_LEADING_NOT |                      \
	 GRN_EXPR_QUERY_NO_SYNTAX_ERROR)

#define PGRN_DEFINE_LOG_LEVEL_ENTRIES(name)                                    \
	static struct config_enum_entry name[] = {                                 \
		{"none", GRN_LOG_NONE, false},                                         \
		{"emergency", GRN_LOG_EMERG, false},                                   \
		{"alert", GRN_LOG_ALERT, false},                                       \
		{"critical", GRN_LOG_CRIT, false},                                     \
		{"error", GRN_LOG_ERROR, false},                                       \
		{"warning", GRN_LOG_WARNING, false},                                   \
		{"notice", GRN_LOG_NOTICE, false},                                     \
		{"info", GRN_LOG_INFO, false},                                         \
		{"debug", GRN_LOG_DEBUG, false},                                       \
		{"dump", GRN_LOG_DUMP, false},                                         \
		{NULL, GRN_LOG_NONE, false}}

extern bool PGrnGroongaInitialized;
void PGrnEnsureDatabase(void);
void PGrnRemoveUnusedTables(void);
bool PGrnIndexIsPGroonga(Relation index);
