/*
 * IDENTIFICATION
 *	pgroonga.h
 */

#ifndef PGROONGA_H
#define PGROONGA_H

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
#define PGrnContainStrategyNumber		7	/* operator %% (@ in Groonga) */
#define PGrnQueryStrategyNumber			8	/* operator @@ (Groonga query) */

/* file and table names */
#define PGrnLogBasename					"pgroonga.log"
#define PGrnDatabaseBasename			"pgrn"
#define PGrnSourcesTableNamePrefix		"Sources"
#define PGrnSourcesTableNameFormat		PGrnSourcesTableNamePrefix "%u"
#define PGrnSourcesCtidColumnName		"ctid"
#define PGrnSourcesCtidColumnNameLength	(sizeof(PGrnSourcesCtidColumnName) - 1)
#define PGrnLexiconNameFormat			"Lexicon%u_%u"
#define PGrnIndexColumnName				"index"

extern void PGDLLEXPORT _PG_init(void);

extern Datum PGDLLEXPORT pgroonga_score(PG_FUNCTION_ARGS);
extern Datum PGDLLEXPORT pgroonga_table_name(PG_FUNCTION_ARGS);
extern Datum PGDLLEXPORT pgroonga_command(PG_FUNCTION_ARGS);
extern Datum PGDLLEXPORT pgroonga_snippet_html(PG_FUNCTION_ARGS);

extern Datum PGDLLEXPORT pgroonga_contain_text(PG_FUNCTION_ARGS);
extern Datum PGDLLEXPORT pgroonga_contain_text_array(PG_FUNCTION_ARGS);
extern Datum PGDLLEXPORT pgroonga_contain_varchar(PG_FUNCTION_ARGS);
extern Datum PGDLLEXPORT pgroonga_contain_varchar_array(PG_FUNCTION_ARGS);
extern Datum PGDLLEXPORT pgroonga_match(PG_FUNCTION_ARGS);

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
extern Datum PGDLLEXPORT pgroonga_costestimate(PG_FUNCTION_ARGS);
extern Datum PGDLLEXPORT pgroonga_options(PG_FUNCTION_ARGS);

#endif	/* PPGROONGA_H */
