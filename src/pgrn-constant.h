#pragma once

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

