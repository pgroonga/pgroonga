#pragma once

#include <postgres.h>
#include <fmgr.h>

/* Default values */
#ifndef PGRN_DEFAULT_TOKENIZER
#  define PGRN_DEFAULT_TOKENIZER "TokenBigram"
#endif
#ifndef PGRN_DEFAULT_NORMALIZER
#  define PGRN_DEFAULT_NORMALIZER "NormalizerAuto"
#endif

/* Groonga strategy types */
#define PGrnLessStrategyNumber			1	/* operator < */
#define PGrnLessEqualStrategyNumber		2	/* operator <= */
#define PGrnEqualStrategyNumber			3	/* operator = */
#define PGrnGreaterEqualStrategyNumber	4	/* operator >= */
#define PGrnGreaterStrategyNumber		5	/* operator > */
#define PGrnLikeStrategyNumber			6	/* operator ~~ (LIKE) */
#define PGrnILikeStrategyNumber			7	/* operator ~~* (ILIKE) */
#define PGrnMatchStrategyNumber			8	/* operator %% (@ in Groonga) */
#define PGrnQueryStrategyNumber			9	/* operator @@ (Groonga query) */
#define PGrnRegexpStrategyNumber		10	/* operator @~ (@~ in Groonga)  */
#define PGrnJSONContainStrategyNumber	11	/* operator @> */

#define PGrnMatchStrategyV2Number		12	/* operator &@	(@ in Groonga) */
#define PGrnQueryStrategyV2Number		13	/* operator &?  (query in Groonga) */
#define PGrnSimilarStrategyV2Number		14	/* operator &~? (similar search) */
#define PGrnScriptStrategyV2Number		15	/* operator &`  (script in Groonga) */
#define PGrnPrefixStrategyV2Number		16	/* operator &^  (prefix search) */
#define PGrnPrefixRKStrategyV2Number	17	/* operator &^~ (prefix RK search) */
#define PGrnMatchContainStrategyV2Number	18	/* operator &@> (@ in Groonga) */
#define PGrnQueryContainStrategyV2Number	19	/* operator &?> (query in Groonga) */
#define PGrnPrefixContainStrategyV2Number	20	/* operator &^>  (prefix search) */
#define PGrnPrefixRKContainStrategyV2Number	21	/* operator &^~> (prefix RK search) */

#define PGRN_N_STRATEGIES PGrnPrefixRKContainStrategyV2Number

/* file and table names */
#define PGrnLogBasename					"pgroonga.log"
#define PGrnQueryLogPathDefault			"none"
#define PGrnDatabaseBasename			"pgrn"
#define PGrnSourcesTableNamePrefix		"Sources"
#define PGrnSourcesTableNamePrefixLength	(sizeof(PGrnSourcesTableNamePrefix) - 1)
#define PGrnSourcesTableNameFormat		PGrnSourcesTableNamePrefix "%u"
#define PGrnSourcesCtidColumnName		"ctid"
#define PGrnSourcesCtidColumnNameLength	(sizeof(PGrnSourcesCtidColumnName) - 1)
#define PGrnJSONPathsTableNamePrefix	"JSONPaths"
#define PGrnJSONPathsTableNameFormat	PGrnJSONPathsTableNamePrefix "%u_%u"
#define PGrnJSONValuesTableNamePrefix	"JSONValues"
#define PGrnJSONValuesTableNameFormat	PGrnJSONValuesTableNamePrefix "%u_%u"
#define PGrnJSONTypesTableNamePrefix	"JSONTypes"
#define PGrnJSONTypesTableNameFormat	PGrnJSONTypesTableNamePrefix "%u_%u"
#define PGrnJSONValueLexiconNameFormat	"JSONValueLexicon%s%u_%u"
#define PGrnLexiconNameFormat			"Lexicon%u_%u"
#define PGrnIndexColumnName				"index"

extern void PGDLLEXPORT _PG_init(void);

extern Datum PGDLLEXPORT pgroonga_score(PG_FUNCTION_ARGS);
extern Datum PGDLLEXPORT pgroonga_table_name(PG_FUNCTION_ARGS);
extern Datum PGDLLEXPORT pgroonga_command(PG_FUNCTION_ARGS);
extern Datum PGDLLEXPORT pgroonga_snippet_html(PG_FUNCTION_ARGS);
extern Datum PGDLLEXPORT pgroonga_highlight_html(PG_FUNCTION_ARGS);
extern Datum PGDLLEXPORT pgroonga_match_positions_byte(PG_FUNCTION_ARGS);
extern Datum PGDLLEXPORT pgroonga_match_positions_character(PG_FUNCTION_ARGS);
extern Datum PGDLLEXPORT pgroonga_query_extract_keywords(PG_FUNCTION_ARGS);
extern Datum PGDLLEXPORT pgroonga_flush(PG_FUNCTION_ARGS);

extern Datum PGDLLEXPORT pgroonga_match_term_text(PG_FUNCTION_ARGS);
extern Datum PGDLLEXPORT pgroonga_match_term_text_array(PG_FUNCTION_ARGS);
extern Datum PGDLLEXPORT pgroonga_match_term_varchar(PG_FUNCTION_ARGS);
extern Datum PGDLLEXPORT pgroonga_match_term_varchar_array(PG_FUNCTION_ARGS);
extern Datum PGDLLEXPORT pgroonga_match_query_text(PG_FUNCTION_ARGS);
extern Datum PGDLLEXPORT pgroonga_match_query_text_array(PG_FUNCTION_ARGS);
extern Datum PGDLLEXPORT pgroonga_match_query_varchar(PG_FUNCTION_ARGS);
extern Datum PGDLLEXPORT pgroonga_match_regexp_text(PG_FUNCTION_ARGS);
extern Datum PGDLLEXPORT pgroonga_match_regexp_varchar(PG_FUNCTION_ARGS);
extern Datum PGDLLEXPORT pgroonga_match_jsonb(PG_FUNCTION_ARGS);

/* v2 */
extern Datum PGDLLEXPORT pgroonga_match_text(PG_FUNCTION_ARGS);
extern Datum PGDLLEXPORT pgroonga_query_text(PG_FUNCTION_ARGS);
extern Datum PGDLLEXPORT pgroonga_similar_text(PG_FUNCTION_ARGS);
extern Datum PGDLLEXPORT pgroonga_script_text(PG_FUNCTION_ARGS);
extern Datum PGDLLEXPORT pgroonga_prefix_text(PG_FUNCTION_ARGS);
extern Datum PGDLLEXPORT pgroonga_prefix_rk_text(PG_FUNCTION_ARGS);
extern Datum PGDLLEXPORT pgroonga_match_contain_text(PG_FUNCTION_ARGS);
extern Datum PGDLLEXPORT pgroonga_query_contain_text(PG_FUNCTION_ARGS);
extern Datum PGDLLEXPORT pgroonga_prefix_contain_text_array(PG_FUNCTION_ARGS);
extern Datum PGDLLEXPORT pgroonga_prefix_rk_contain_text_array(PG_FUNCTION_ARGS);

extern Datum PGDLLEXPORT pgroonga_insert(PG_FUNCTION_ARGS);
extern Datum PGDLLEXPORT pgroonga_beginscan(PG_FUNCTION_ARGS);
extern Datum PGDLLEXPORT pgroonga_gettuple(PG_FUNCTION_ARGS);
extern Datum PGDLLEXPORT pgroonga_getbitmap(PG_FUNCTION_ARGS);
extern Datum PGDLLEXPORT pgroonga_rescan(PG_FUNCTION_ARGS);
extern Datum PGDLLEXPORT pgroonga_endscan(PG_FUNCTION_ARGS);
extern Datum PGDLLEXPORT pgroonga_build(PG_FUNCTION_ARGS);
extern Datum PGDLLEXPORT pgroonga_buildempty(PG_FUNCTION_ARGS);
extern Datum PGDLLEXPORT pgroonga_bulkdelete(PG_FUNCTION_ARGS);
extern Datum PGDLLEXPORT pgroonga_vacuumcleanup(PG_FUNCTION_ARGS);
extern Datum PGDLLEXPORT pgroonga_canreturn(PG_FUNCTION_ARGS);
extern Datum PGDLLEXPORT pgroonga_costestimate(PG_FUNCTION_ARGS);
extern Datum PGDLLEXPORT pgroonga_options(PG_FUNCTION_ARGS);
extern Datum PGDLLEXPORT pgroonga_handler(PG_FUNCTION_ARGS);
