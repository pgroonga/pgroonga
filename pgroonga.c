#include "pgroonga.h"

#include <access/reloptions.h>
#include <access/relscan.h>
#include <catalog/catalog.h>
#include <catalog/index.h>
#include <catalog/pg_tablespace.h>
#include <catalog/pg_type.h>
#include <lib/ilist.h>
#include <mb/pg_wchar.h>
#include <miscadmin.h>
#include <optimizer/cost.h>
#include <storage/bufmgr.h>
#include <storage/ipc.h>
#include <storage/lmgr.h>
#include <utils/array.h>
#include <utils/builtins.h>
#include <utils/guc.h>
#include <utils/lsyscache.h>
#include <utils/selfuncs.h>
#include <utils/snapmgr.h>
#include <utils/syscache.h>
#include <utils/timestamp.h>
#include <utils/tqual.h>
#include <utils/typcache.h>

#include <groonga.h>

#include <stdlib.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifndef WIN32
#	include <unistd.h>
#endif

#ifdef WIN32
typedef struct _stat pgrn_stat_buffer;
#	define pgrn_stat(path, buffer) _stat(path, buffer)
#else
typedef struct stat pgrn_stat_buffer;
#	define pgrn_stat(path, buffer) stat(path, buffer)
#endif

#define VARCHARARRAYOID 1015

PG_MODULE_MAGIC;

static bool PGrnIsLZ4Available;
static relopt_kind PGrnReloptionKind;

static int PGrnLogType;
enum PGrnLogType {
	PGRN_LOG_TYPE_FILE,
	PGRN_LOG_TYPE_WINDOWS_EVENT_LOG,
	PGRN_LOG_TYPE_POSTGRESQL
};
static struct config_enum_entry PGrnLogTypeEntries[] = {
	{"file",              PGRN_LOG_TYPE_FILE,              false},
	{"windows_event_log", PGRN_LOG_TYPE_WINDOWS_EVENT_LOG, false},
	{"postgresql",        PGRN_LOG_TYPE_POSTGRESQL,        false},
	{NULL,                PGRN_LOG_TYPE_FILE,              false}
};

static char *PGrnLogPath;

static int PGrnLogLevel;
static struct config_enum_entry PGrnLogLevelEntries[] = {
	{"none",      GRN_LOG_NONE,    false},
	{"emergency", GRN_LOG_EMERG,   false},
	{"alert",     GRN_LOG_ALERT,   false},
	{"critical",  GRN_LOG_CRIT,    false},
	{"error",     GRN_LOG_ERROR,   false},
	{"warning",   GRN_LOG_WARNING, false},
	{"notice",    GRN_LOG_NOTICE,  false},
	{"info",      GRN_LOG_INFO,    false},
	{"debug",     GRN_LOG_DEBUG,   false},
	{"dump",      GRN_LOG_DUMP,    false},
	{NULL,        GRN_LOG_NONE,    false}
};

static int PGrnLockTimeout;

typedef struct PGrnOptions
{
	int32 vl_len_;
	int tokenizerOffset;
	int normalizerOffset;
} PGrnOptions;

typedef struct PGrnCreateData
{
	Relation index;
	grn_obj *sourcesTable;
	grn_obj *sourcesCtidColumn;
	grn_obj *lexicons;
	unsigned int i;
	TupleDesc desc;
	Oid relNode;
	bool forFullTextSearch;
	grn_id attributeTypeID;
	unsigned char attributeFlags;
} PGrnCreateData;

typedef struct PGrnBuildStateData
{
	grn_obj	*sourcesTable;
	grn_obj	*sourcesCtidColumn;
	double nIndexedTuples;
} PGrnBuildStateData;

typedef PGrnBuildStateData *PGrnBuildState;

typedef struct PGrnScanOpaqueData
{
	slist_node node;
	Oid dataTableID;
	struct
	{
		AttrNumber number;
		Oid type;
		grn_id domain;
		unsigned char flags;
		grn_obj *lexicon;
		grn_obj *indexColumn;
	} primaryKey;
	grn_obj *sourcesTable;
	grn_obj *sourcesCtidColumn;
	grn_obj minBorderValue;
	grn_obj maxBorderValue;
	grn_obj *searched;
	grn_obj *sorted;
	grn_obj *targetTable;
	grn_obj *indexCursor;
	grn_table_cursor *tableCursor;
	grn_obj *ctidAccessor;
	grn_obj *scoreAccessor;
	grn_id currentID;
} PGrnScanOpaqueData;

typedef PGrnScanOpaqueData *PGrnScanOpaque;

typedef struct PGrnSearchData
{
	grn_obj targetColumns;
	grn_obj matchTargets;
	grn_obj sectionID;
	grn_obj *expression;
	grn_obj *expressionVariable;
	bool    isEmptyCondition;
} PGrnSearchData;

static slist_head PGrnScanOpaques = SLIST_STATIC_INIT(PGrnScanOpaques);

PG_FUNCTION_INFO_V1(pgroonga_score);
PG_FUNCTION_INFO_V1(pgroonga_table_name);
PG_FUNCTION_INFO_V1(pgroonga_command);
PG_FUNCTION_INFO_V1(pgroonga_snippet_html);

PG_FUNCTION_INFO_V1(pgroonga_contain_text);
PG_FUNCTION_INFO_V1(pgroonga_contain_text_array);
PG_FUNCTION_INFO_V1(pgroonga_contain_varchar);
PG_FUNCTION_INFO_V1(pgroonga_contain_varchar_array);
PG_FUNCTION_INFO_V1(pgroonga_match);

PG_FUNCTION_INFO_V1(pgroonga_insert);
PG_FUNCTION_INFO_V1(pgroonga_beginscan);
PG_FUNCTION_INFO_V1(pgroonga_gettuple);
PG_FUNCTION_INFO_V1(pgroonga_getbitmap);
PG_FUNCTION_INFO_V1(pgroonga_rescan);
PG_FUNCTION_INFO_V1(pgroonga_endscan);
PG_FUNCTION_INFO_V1(pgroonga_build);
PG_FUNCTION_INFO_V1(pgroonga_buildempty);
PG_FUNCTION_INFO_V1(pgroonga_bulkdelete);
PG_FUNCTION_INFO_V1(pgroonga_vacuumcleanup);
PG_FUNCTION_INFO_V1(pgroonga_costestimate);
PG_FUNCTION_INFO_V1(pgroonga_options);

static grn_ctx grnContext;
static grn_ctx *ctx = &grnContext;
static grn_obj buffer;
static grn_obj ctidBuffer;
static grn_obj scoreBuffer;
static grn_obj headBuffer;
static grn_obj bodyBuffer;
static grn_obj footBuffer;
static grn_obj inspectBuffer;

static const char *
PGrnInspect(grn_obj *object)
{
	GRN_BULK_REWIND(&inspectBuffer);
	grn_inspect(ctx, &inspectBuffer, object);
	GRN_TEXT_PUTC(ctx, &inspectBuffer, '\0');
	return GRN_TEXT_VALUE(&inspectBuffer);
}

static grn_encoding
PGrnGetEncoding(void)
{
	int	enc = GetDatabaseEncoding();

	switch (enc)
	{
	case PG_SQL_ASCII:
	case PG_UTF8:
		return GRN_ENC_UTF8;
	case PG_EUC_JP:
	case PG_EUC_JIS_2004:
		return GRN_ENC_EUC_JP;
	case PG_LATIN1:
		return GRN_ENC_LATIN1;
	case PG_KOI8R:
		return GRN_ENC_KOI8R;
	case PG_SJIS:
	case PG_SHIFT_JIS_2004:
		return GRN_ENC_SJIS;
	default:
		elog(WARNING,
			 "pgroonga: use default encoding instead of '%s'",
			 GetDatabaseEncodingName());
		return GRN_ENC_DEFAULT;
	}
}

static void
PGrnPostgreSQLLoggerLog(grn_ctx *ctx, grn_log_level level,
						const char *timestamp, const char *title,
						const char *message, const char *location,
						void *user_data)
{
	const char levelMarks[] = " EACewnid-";

	if (location && location[0])
	{
		ereport(LOG,
				(errmsg("pgroonga:log: %s|%c|%s %s %s",
						timestamp, levelMarks[level], title,
						message, location)));
	}
	else
	{
		ereport(LOG,
				(errmsg("pgroonga:log: %s|%c|%s %s",
						timestamp, levelMarks[level], title, message)));
	}
}

static grn_logger PGrnPostgreSQLLogger = {
	GRN_LOG_DEFAULT_LEVEL,
	GRN_LOG_TIME | GRN_LOG_MESSAGE,
	NULL,
	PGrnPostgreSQLLoggerLog,
	NULL,
	NULL
};

static void
PGrnLogTypeAssign(int new_value, void *extra)
{
	switch (new_value) {
	case PGRN_LOG_TYPE_WINDOWS_EVENT_LOG:
		grn_windows_event_logger_set(ctx, "PGroonga");
		break;
	case PGRN_LOG_TYPE_POSTGRESQL:
		grn_logger_set(ctx, &PGrnPostgreSQLLogger);
		break;
	default:
		grn_logger_set(ctx, NULL);
		break;
	}
}

static void
PGrnLogPathAssign(const char *new_value, void *extra)
{
	if (new_value) {
		if (strcmp(new_value, "none") == 0) {
			grn_default_logger_set_path(NULL);
		} else {
			grn_default_logger_set_path(new_value);
		}
	} else {
		grn_default_logger_set_path(PGrnLogBasename);
	}
}

static void
PGrnLogLevelAssign(int new_value, void *extra)
{
	grn_default_logger_set_max_level(new_value);
}

static void
PGrnLockTimeoutAssign(int new_value, void *extra)
{
	grn_set_lock_timeout(new_value);
}

static void
PGrnInitializeVariables(void)
{
	DefineCustomEnumVariable("pgroonga.log_type",
							 "Log type for PGroonga.",
							 "Available log types: "
							 "[file, windows_event_log, postgresql]. "
							 "The default is file.",
							 &PGrnLogType,
							 PGRN_LOG_TYPE_FILE,
							 PGrnLogTypeEntries,
							 PGC_USERSET,
							 0,
							 NULL,
							 PGrnLogTypeAssign,
							 NULL);

	DefineCustomStringVariable("pgroonga.log_path",
							   "Log path for PGroonga.",
							   "The default is "
							   "\"${PG_DATA}/" PGrnLogBasename "\". "
							   "Use \"none\" to disable file output.",
							   &PGrnLogPath,
							   NULL,
							   PGC_USERSET,
							   0,
							   NULL,
							   PGrnLogPathAssign,
							   NULL);

	DefineCustomEnumVariable("pgroonga.log_level",
							 "Log level for PGroonga.",
							 "Available log levels: "
							 "[none, emergency, alert, critical, "
							 "error, warning, notice, info, debug, dump]. "
							 "The default is notice.",
							 &PGrnLogLevel,
							 GRN_LOG_DEFAULT_LEVEL,
							 PGrnLogLevelEntries,
							 PGC_USERSET,
							 0,
							 NULL,
							 PGrnLogLevelAssign,
							 NULL);

	DefineCustomIntVariable("pgroonga.lock_timeout",
							"Try pgroonga.lock_timeout times "
							"at 1 msec intervals to "
							"get write lock in PGroonga.",
							"The default is 10000000. "
							"It means that PGroonga tries to get write lock "
							"between about 2.7 hours.",
							&PGrnLockTimeout,
							grn_get_lock_timeout(),
							0,
							INT_MAX,
							PGC_USERSET,
							0,
							NULL,
							PGrnLockTimeoutAssign,
							NULL);

	EmitWarningsOnPlaceholders("pgroonga");
}

static void
PGrnEnsureDatabase(void)
{
	char *databasePath;
	char path[MAXPGPATH];
	grn_obj	*db;
	pgrn_stat_buffer file_status;

	GRN_CTX_SET_ENCODING(ctx, PGrnGetEncoding());
	databasePath = GetDatabasePath(MyDatabaseId, DEFAULTTABLESPACE_OID);
	join_path_components(path,
						 databasePath,
						 PGrnDatabaseBasename);
	pfree(databasePath);

	if (pgrn_stat(path, &file_status) == 0)
	{
		db = grn_db_open(ctx, path);
		if (!db)
			ereport(ERROR,
					(errcode(ERRCODE_IO_ERROR),
					 errmsg("pgroonga: failed to open database: <%s>: %s",
							path, ctx->errbuf)));
	}
	else
	{
		db = grn_db_create(ctx, path, NULL);
		if (!db)
			ereport(ERROR,
					(errcode(ERRCODE_IO_ERROR),
					 errmsg("pgroonga: failed to create database: <%s>: %s",
							path, ctx->errbuf)));
	}
}

static void
PGrnOnProcExit(int code, Datum arg)
{
	grn_obj *db;

	GRN_OBJ_FIN(ctx, &inspectBuffer);
	GRN_OBJ_FIN(ctx, &footBuffer);
	GRN_OBJ_FIN(ctx, &bodyBuffer);
	GRN_OBJ_FIN(ctx, &headBuffer);
	GRN_OBJ_FIN(ctx, &ctidBuffer);
	GRN_OBJ_FIN(ctx, &scoreBuffer);
	GRN_OBJ_FIN(ctx, &buffer);

	db = grn_ctx_db(ctx);
	if (db)
		grn_obj_close(ctx, db);

	grn_ctx_fin(ctx);
	grn_fin();
}

static void
PGrnInitializeGroongaInformation(void)
{
	grn_obj grnIsSupported;

	GRN_BOOL_INIT(&grnIsSupported, 0);
	grn_obj_get_info(ctx, NULL, GRN_INFO_SUPPORT_LZ4, &grnIsSupported);
	PGrnIsLZ4Available = (GRN_BOOL_VALUE(&grnIsSupported));
	GRN_OBJ_FIN(ctx, &grnIsSupported);
}

static bool
PGrnIsTokenizer(grn_obj *object)
{
	if (object->header.type != GRN_PROC)
		return false;

  if (grn_proc_get_type(ctx, object) != GRN_PROC_TOKENIZER)
	  return false;

  return true;
}

static void
PGrnOptionValidateTokenizer(char *name)
{
	grn_obj *tokenizer;
	size_t name_length;

	name_length = strlen(name);
	if (name_length == 0)
		return;

	tokenizer = grn_ctx_get(ctx, name, name_length);
	if (!tokenizer)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("pgroonga: nonexistent tokenizer: <%s>",
						name)));
	}

	if (!PGrnIsTokenizer(tokenizer))
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("pgroonga: not tokenizer: <%s>: %s",
						name, PGrnInspect(tokenizer))));
	}
}

static bool
PGrnIsNormalizer(grn_obj *object)
{
	if (object->header.type != GRN_PROC)
		return false;

  if (grn_proc_get_type(ctx, object) != GRN_PROC_NORMALIZER)
	  return false;

  return true;
}

static void
PGrnOptionValidateNormalizer(char *name)
{
	grn_obj *normalizer;
	size_t name_length;

	name_length = strlen(name);
	if (name_length == 0)
		return;

	normalizer = grn_ctx_get(ctx, name, name_length);
	if (!normalizer)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("pgroonga: nonexistent normalizer: <%s>",
						name)));
	}

	if (!PGrnIsNormalizer(normalizer))
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("pgroonga: not normalizer: <%s>: %s",
						name, PGrnInspect(normalizer))));
	}
}

static void
PGrnInitializeOptions(void)
{
	PGrnReloptionKind = add_reloption_kind();

	add_string_reloption(PGrnReloptionKind,
						 "tokenizer",
						 "Tokenizer name to be used for full-text search",
						 PGRN_DEFAULT_TOKENIZER,
						 PGrnOptionValidateTokenizer);
	add_string_reloption(PGrnReloptionKind,
						 "normalizer",
						 "Normalizer name to be used for full-text search",
						 PGRN_DEFAULT_NORMALIZER,
						 PGrnOptionValidateNormalizer);
}

void
_PG_init(void)
{
	PGrnInitializeVariables();

	if (grn_init() != GRN_SUCCESS)
		ereport(ERROR,
				(errcode(ERRCODE_SYSTEM_ERROR),
				 errmsg("pgroonga: failed to initialize Groonga")));
	if (grn_ctx_init(ctx, 0))
		ereport(ERROR,
				(errcode(ERRCODE_SYSTEM_ERROR),
				 errmsg("pgroonga: failed to initialize Groonga context")));

	on_proc_exit(PGrnOnProcExit, 0);

	GRN_VOID_INIT(&buffer);
	GRN_FLOAT_INIT(&scoreBuffer, 0);
	GRN_UINT64_INIT(&ctidBuffer, 0);
	GRN_TEXT_INIT(&headBuffer, 0);
	GRN_TEXT_INIT(&bodyBuffer, 0);
	GRN_TEXT_INIT(&footBuffer, 0);
	GRN_TEXT_INIT(&inspectBuffer, 0);

	PGrnEnsureDatabase();

	PGrnInitializeGroongaInformation();

	PGrnInitializeOptions();
}

static int
PGrnRCToPgErrorCode(grn_rc rc)
{
	int errorCode = ERRCODE_SYSTEM_ERROR;

	/* TODO: Fill me. */
	switch (rc)
	{
	case GRN_NO_SUCH_FILE_OR_DIRECTORY:
		errorCode = ERRCODE_IO_ERROR;
		break;
	case GRN_INPUT_OUTPUT_ERROR:
		errorCode = ERRCODE_IO_ERROR;
		break;
	case GRN_INVALID_ARGUMENT:
		errorCode = ERRCODE_INVALID_PARAMETER_VALUE;
		break;
	default:
		break;
	}

	return errorCode;
}

static grn_bool
PGrnCheck(const char *message)
{
	if (ctx->rc == GRN_SUCCESS)
		return GRN_TRUE;

	ereport(ERROR,
			(errcode(PGrnRCToPgErrorCode(ctx->rc)),
			 errmsg("pgroonga: %s: %s", message, ctx->errbuf)));
	return GRN_FALSE;
}

static grn_id
PGrnGetType(Relation index, AttrNumber n, unsigned char *flags)
{
	TupleDesc desc = RelationGetDescr(index);
	Form_pg_attribute attr;
	grn_id typeID = GRN_ID_NIL;
	unsigned char typeFlags = 0;
	int32 maxLength;

	attr = desc->attrs[n];

	switch (attr->atttypid)
	{
	case BOOLOID:
		typeID = GRN_DB_BOOL;
		break;
	case INT2OID:
		typeID = GRN_DB_INT16;
		break;
	case INT4OID:
		typeID = GRN_DB_INT32;
		break;
	case INT8OID:
		typeID = GRN_DB_INT64;
		break;
	case FLOAT4OID:
	case FLOAT8OID:
		typeID = GRN_DB_FLOAT;
		break;
	case TIMESTAMPOID:
	case TIMESTAMPTZOID:
		typeID = GRN_DB_TIME;
		break;
	case TEXTOID:
	case XMLOID:
		typeID = GRN_DB_LONG_TEXT;
		break;
	case VARCHAROID:
		maxLength = type_maximum_size(attr->atttypid, attr->atttypmod);
		if (maxLength > 4096)
		{
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("pgroonga: "
							"4097bytes over size varchar isn't supported: %d",
							maxLength)));
		}
		typeID = GRN_DB_SHORT_TEXT;	/* 4KB */
		break;
#ifdef NOT_USED
	case POINTOID:
		typeID = GRN_DB_TOKYO_GEO_POINT or GRN_DB_WGS84_GEO_POINT;
		break;
#endif
	case VARCHARARRAYOID:
		maxLength = type_maximum_size(VARCHAROID, attr->atttypmod);
		if (maxLength > 4096)
		{
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("pgroonga: "
							"array of 4097bytes over size varchar "
							"isn't supported: %d",
							maxLength)));
		}
		typeID = GRN_DB_SHORT_TEXT;
		typeFlags |= GRN_OBJ_VECTOR;
		break;
	case TEXTARRAYOID:
		typeID = GRN_DB_LONG_TEXT;
		typeFlags |= GRN_OBJ_VECTOR;
		break;
	default:
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("pgroonga: unsupported type: %u", attr->atttypid)));
		break;
	}

	if (flags)
	{
		*flags = typeFlags;
	}

	return typeID;
}

static void
PGrnConvertDatumArrayType(Datum datum, Oid typeID, grn_obj *buffer)
{
	ArrayType *value = DatumGetArrayTypeP(datum);
	int i, n;

	n = ARR_DIMS(value)[0];
	for (i = 1; i <= n; i++)
	{
		int weight = 0;
		Datum elementDatum;
		VarChar *element;
		bool isNULL;

		elementDatum = array_ref(value, 1, &i, -1, -1, false, 'i', &isNULL);
		if (isNULL)
			continue;

		switch (typeID)
		{
		case VARCHARARRAYOID:
			element = DatumGetVarCharPP(elementDatum);
			grn_vector_add_element(ctx, buffer,
								   VARDATA_ANY(element),
								   VARSIZE_ANY_EXHDR(element),
								   weight,
								   buffer->header.domain);
			break;
		case TEXTARRAYOID:
			element = DatumGetTextPP(elementDatum);
			grn_vector_add_element(ctx, buffer,
								   VARDATA_ANY(element),
								   VARSIZE_ANY_EXHDR(element),
								   weight,
								   buffer->header.domain);
			break;
		}
	}
}

static void
PGrnConvertDatum(Datum datum, Oid typeID, grn_obj *buffer)
{
	switch (typeID)
	{
	case BOOLOID:
		GRN_BOOL_SET(ctx, buffer, DatumGetBool(datum));
		break;
	case INT2OID:
		GRN_INT16_SET(ctx, buffer, DatumGetInt16(datum));
		break;
	case INT4OID:
		GRN_INT32_SET(ctx, buffer, DatumGetInt32(datum));
		break;
	case INT8OID:
		GRN_INT64_SET(ctx, buffer, DatumGetInt64(datum));
		break;
	case FLOAT4OID:
		GRN_FLOAT_SET(ctx, buffer, DatumGetFloat4(datum));
		break;
	case FLOAT8OID:
		GRN_FLOAT_SET(ctx, buffer, DatumGetFloat8(datum));
		break;
	case TIMESTAMPOID:
	case TIMESTAMPTZOID:
	{
		Timestamp value = DatumGetTimestamp(datum);
		pg_time_t unixTime;
		int32 usec;

		unixTime = timestamptz_to_time_t(value);
#ifdef HAVE_INT64_TIMESTAMP
		usec = value % USECS_PER_SEC;
#else
		{
			double rawUsec;
			modf(value, &rawUsec);
			usec = rawUsec * USECS_PER_SEC;
			if (usec < 0.0)
			{
				usec = -usec;
			}
		}
#endif
		GRN_TIME_SET(ctx, buffer, GRN_TIME_PACK(unixTime, usec));
		break;
	}
	case TEXTOID:
	case XMLOID:
	{
		text *value = DatumGetTextPP(datum);
		GRN_TEXT_SET(ctx, buffer,
					 VARDATA_ANY(value), VARSIZE_ANY_EXHDR(value));
		break;
	}
	case VARCHAROID:
	{
		VarChar *value = DatumGetVarCharPP(datum);
		GRN_TEXT_SET(ctx, buffer,
					 VARDATA_ANY(value), VARSIZE_ANY_EXHDR(value));
		break;
	}
#ifdef NOT_USED
	case POINTOID:
		/* GRN_DB_TOKYO_GEO_POINT or GRN_DB_WGS84_GEO_POINT; */
		break;
#endif
	case VARCHARARRAYOID:
	case TEXTARRAYOID:
		PGrnConvertDatumArrayType(datum, typeID, buffer);
		break;
	default:
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("pgroonga: unsupported datum type: %u",
						typeID)));
		break;
	}
}

static grn_obj *
PGrnLookup(const char *name, int errorLevel)
{
	grn_obj *object = grn_ctx_get(ctx, name, strlen(name));
	if (!object)
		ereport(errorLevel,
				(errcode(ERRCODE_INVALID_NAME),
				 errmsg("pgroonga: object isn't found: <%s>", name)));
	return object;
}

static grn_obj *
PGrnLookupSourcesTable(Relation index, int errorLevel)
{
	char name[GRN_TABLE_MAX_KEY_SIZE];

	snprintf(name, sizeof(name),
			 PGrnSourcesTableNameFormat,
			 index->rd_node.relNode);
	return PGrnLookup(name, errorLevel);
}

static grn_obj *
PGrnLookupSourcesCtidColumn(Relation index, int errorLevel)
{
	char name[GRN_TABLE_MAX_KEY_SIZE];

	snprintf(name, sizeof(name),
			 PGrnSourcesTableNameFormat "." PGrnSourcesCtidColumnName,
			 index->rd_node.relNode);
	return PGrnLookup(name, errorLevel);
}

static grn_obj *
PGrnLookupIndexColumn(Relation index, unsigned int nthAttribute, int errorLevel)
{
	char name[GRN_TABLE_MAX_KEY_SIZE];

	snprintf(name, sizeof(name),
			 PGrnLexiconNameFormat ".%s",
			 index->rd_node.relNode,
			 nthAttribute,
			 PGrnIndexColumnName);
	return PGrnLookup(name, errorLevel);
}

static grn_obj *
PGrnCreateTable(const char *name,
				grn_obj_flags flags,
				grn_obj *type)
{
	grn_obj	*table;

	table = grn_table_create(ctx,
							 name, strlen(name), NULL,
							 GRN_OBJ_PERSISTENT | flags,
							 type,
							 NULL);
	PGrnCheck("pgroonga: failed to create table");

	return table;
}

static grn_obj *
PGrnCreateColumn(grn_obj	*table,
				 const char *name,
				 grn_obj_flags flags,
				 grn_obj	*type)
{
	grn_obj *column;

    column = grn_column_create(ctx, table,
							   name, strlen(name), NULL,
							   GRN_OBJ_PERSISTENT | flags,
							   type);
	PGrnCheck("pgroonga: failed to create column");

	return column;
}

static bool
PGrnIsForFullTextSearchIndex(Relation index, int nthAttribute)
{
	Oid queryStrategyOID;
	Oid leftType;
	Oid rightType;

	leftType = index->rd_opcintype[nthAttribute];
	switch (leftType)
	{
	case VARCHARARRAYOID:
		rightType = VARCHAROID;
		break;
	case TEXTARRAYOID:
		rightType = TEXTOID;
		break;
	default:
		rightType = leftType;
		break;
	}
	queryStrategyOID = get_opfamily_member(index->rd_opfamily[nthAttribute],
										   leftType,
										   rightType,
										   PGrnQueryStrategyNumber);
	return OidIsValid(queryStrategyOID);
}

static void
PGrnCreateSourcesCtidColumn(PGrnCreateData *data)
{
	data->sourcesCtidColumn = PGrnCreateColumn(data->sourcesTable,
											   PGrnSourcesCtidColumnName,
											   GRN_OBJ_COLUMN_SCALAR,
											   grn_ctx_at(ctx, GRN_DB_UINT64));
}

static void
PGrnCreateSourcesTable(PGrnCreateData *data)
{
	char sourcesTableName[GRN_TABLE_MAX_KEY_SIZE];

	snprintf(sourcesTableName, sizeof(sourcesTableName),
			 PGrnSourcesTableNameFormat, data->relNode);
	data->sourcesTable = PGrnCreateTable(sourcesTableName,
										 GRN_OBJ_TABLE_NO_KEY,
										 NULL);

	PGrnCreateSourcesCtidColumn(data);
}

static void
PGrnCreateDataColumn(PGrnCreateData *data)
{
	grn_obj_flags flags = 0;

	if (data->attributeFlags & GRN_OBJ_VECTOR)
	{
		flags |= GRN_OBJ_COLUMN_VECTOR;
	}
	else
	{
		flags |= GRN_OBJ_COLUMN_SCALAR;

		if (PGrnIsLZ4Available)
		{
			switch (data->attributeTypeID)
			{
			case GRN_DB_SHORT_TEXT:
			case GRN_DB_TEXT:
			case GRN_DB_LONG_TEXT:
				flags |= GRN_OBJ_COMPRESS_LZ4;
				break;
			}
		}
	}

	PGrnCreateColumn(data->sourcesTable,
					 data->desc->attrs[data->i]->attname.data,
					 flags,
					 grn_ctx_at(ctx, data->attributeTypeID));
}

static void
PGrnCreateIndexColumn(PGrnCreateData *data)
{
	grn_id typeID = GRN_ID_NIL;
	char lexiconName[GRN_TABLE_MAX_KEY_SIZE];
	grn_obj *lexicon;

	switch (data->attributeTypeID)
	{
	case GRN_DB_TEXT:
	case GRN_DB_LONG_TEXT:
		typeID = GRN_DB_SHORT_TEXT;
		break;
	default:
		typeID = data->attributeTypeID;
		break;
	}

	snprintf(lexiconName, sizeof(lexiconName),
			 PGrnLexiconNameFormat, data->relNode, data->i);
	lexicon = PGrnCreateTable(lexiconName,
							  GRN_OBJ_TABLE_PAT_KEY,
							  grn_ctx_at(ctx, typeID));
	GRN_PTR_PUT(ctx, data->lexicons, lexicon);

	if (data->forFullTextSearch)
	{
		PGrnOptions *options;
		const char *tokenizerName = PGRN_DEFAULT_TOKENIZER;
		const char *normalizerName = PGRN_DEFAULT_NORMALIZER;

		options = (PGrnOptions *) (data->index->rd_options);
		if (options)
		{
			tokenizerName = (const char *) (options) + options->tokenizerOffset;
			normalizerName = (const char *) (options) + options->normalizerOffset;
		}
		if (tokenizerName && tokenizerName[0])
		{
			grn_obj_set_info(ctx, lexicon, GRN_INFO_DEFAULT_TOKENIZER,
							 PGrnLookup(tokenizerName, ERROR));
		}
		if (normalizerName && normalizerName[0])
		{
			grn_obj_set_info(ctx, lexicon, GRN_INFO_NORMALIZER,
							 PGrnLookup(normalizerName, ERROR));
		}
	}

	{
		grn_obj_flags flags = GRN_OBJ_COLUMN_INDEX;
		if (data->forFullTextSearch)
			flags |= GRN_OBJ_WITH_POSITION;
		PGrnCreateColumn(lexicon,
						 PGrnIndexColumnName,
						 flags,
						 data->sourcesTable);
	}
}

/**
 * PGrnCreate
 *
 * @param	ctx
 * @param	index
 */
static void
PGrnCreate(Relation index,
		   grn_obj **sourcesTable,
		   grn_obj **sourcesCtidColumn,
		   grn_obj *lexicons)
{
	PGrnCreateData data;

	data.index = index;
	data.desc = RelationGetDescr(index);
	data.relNode = index->rd_node.relNode;
	data.lexicons = lexicons;

	PGrnCreateSourcesTable(&data);
	*sourcesTable = data.sourcesTable;
	*sourcesCtidColumn = data.sourcesCtidColumn;

	for (data.i = 0; data.i < data.desc->natts; data.i++)
	{
		data.forFullTextSearch = PGrnIsForFullTextSearchIndex(index, data.i);
		data.attributeTypeID = PGrnGetType(index, data.i,
										   &(data.attributeFlags));
		PGrnCreateDataColumn(&data);
		PGrnCreateIndexColumn(&data);
	}
}

static void
PGrnSetSources(Relation index, grn_obj *sourcesTable)
{
	TupleDesc desc;
	grn_obj sourceIDs;
	int i;

	desc = RelationGetDescr(index);
	GRN_RECORD_INIT(&sourceIDs, GRN_OBJ_VECTOR, GRN_ID_NIL);
	for (i = 0; i < desc->natts; i++)
	{
		NameData *name = &(desc->attrs[i]->attname);
		grn_obj *source;
		grn_id sourceID;
		grn_obj *indexColumn;

		GRN_BULK_REWIND(&sourceIDs);

		source = grn_obj_column(ctx, sourcesTable,
								name->data, strlen(name->data));
		sourceID = grn_obj_id(ctx, source);
		grn_obj_unlink(ctx, source);
		GRN_RECORD_PUT(ctx, &sourceIDs, sourceID);

		indexColumn = PGrnLookupIndexColumn(index, i, ERROR);
		grn_obj_set_info(ctx, indexColumn, GRN_INFO_SOURCE, &sourceIDs);
	}
	GRN_OBJ_FIN(ctx, &sourceIDs);
}

static uint64
CtidToUInt64(ItemPointer ctid)
{
	BlockNumber blockNumber;
	OffsetNumber offsetNumber;

	blockNumber = ItemPointerGetBlockNumber(ctid);
	offsetNumber = ItemPointerGetOffsetNumber(ctid);
	return (((uint64)blockNumber << 16) | ((uint64)offsetNumber));
}

static ItemPointerData
UInt64ToCtid(uint64 key)
{
	ItemPointerData	ctid;
	ItemPointerSet(&ctid, (key >> 16) & 0xFFFFFFFF, key & 0xFFFF);
	return ctid;
}

static bool
PGrnIsAliveCtid(Relation table, ItemPointer ctid)
{
	Buffer buffer;
	HeapTupleData tuple;
	Snapshot snapshot;
	ItemPointerData realCtid;
	bool allDead;
	bool found;
	bool isAlive = false;

	buffer = ReadBuffer(table, ItemPointerGetBlockNumber(ctid));
	snapshot = RegisterSnapshot(GetLatestSnapshot());
	realCtid = *ctid;
	found = heap_hot_search_buffer(&realCtid, table, buffer, snapshot, &tuple,
								   &allDead, true);
	if (found) {
		uint64 tupleID;

		tupleID = CtidToUInt64(&(tuple.t_self));
		isAlive = (tupleID == CtidToUInt64(ctid) ||
				   tupleID == CtidToUInt64(&realCtid));
	}
	UnregisterSnapshot(snapshot);
	ReleaseBuffer(buffer);

	return isAlive;
}

static double
PGrnCollectScoreScanOpaque(Relation table, HeapTuple tuple, PGrnScanOpaque so)
{
	double score = 0.0;
	TupleDesc desc;
	bool isNULL;
	Datum primaryKeyValue;
	grn_table_cursor *tableCursor;
	grn_obj *indexCursor;
	grn_id recordID;

	if (so->dataTableID != tuple->t_tableOid)
		return 0.0;

	if (!so->scoreAccessor)
		return 0.0;

	if (!OidIsValid(so->primaryKey.type))
		return 0.0;

	grn_obj_reinit(ctx, &buffer, so->primaryKey.domain, so->primaryKey.flags);

	desc = RelationGetDescr(table);
	primaryKeyValue = heap_getattr(tuple, so->primaryKey.number, desc, &isNULL);
	PGrnConvertDatum(primaryKeyValue, so->primaryKey.type, &buffer);

	tableCursor = grn_table_cursor_open(ctx, so->primaryKey.lexicon,
										GRN_BULK_HEAD(&buffer),
										GRN_BULK_VSIZE(&buffer),
										GRN_BULK_HEAD(&buffer),
										GRN_BULK_VSIZE(&buffer),
										0, -1, GRN_CURSOR_ASCENDING);
	if (!tableCursor)
		return 0.0;


	indexCursor = grn_index_cursor_open(ctx,
										tableCursor,
										so->primaryKey.indexColumn,
										GRN_ID_NIL, GRN_ID_MAX, 0);
	if (!indexCursor)
	{
		grn_table_cursor_close(ctx, tableCursor);
		return 0.0;
	}

	while ((recordID = grn_table_cursor_next(ctx, indexCursor)) != GRN_ID_NIL)
	{
		grn_id id;
		ItemPointerData ctid;

		id = grn_table_get(ctx, so->searched, &recordID, sizeof(grn_id));
		if (id == GRN_ID_NIL)
			continue;

		GRN_BULK_REWIND(&ctidBuffer);
		grn_obj_get_value(ctx, so->ctidAccessor, id, &ctidBuffer);
		ctid = UInt64ToCtid(GRN_UINT64_VALUE(&ctidBuffer));

		if (!PGrnIsAliveCtid(table, &ctid))
			continue;

		GRN_BULK_REWIND(&scoreBuffer);
		grn_obj_get_value(ctx, so->scoreAccessor, id, &scoreBuffer);
		if (scoreBuffer.header.domain == GRN_DB_FLOAT)
		{
			score += GRN_FLOAT_VALUE(&scoreBuffer);
		}
		else
		{
			score += GRN_INT32_VALUE(&scoreBuffer);
		}
	}

	grn_obj_unlink(ctx, indexCursor);
	grn_obj_unlink(ctx, tableCursor);

	return score;
}

static double
PGrnCollectScore(Relation table, HeapTuple tuple)
{
	double score = 0.0;
	slist_iter iter;

	slist_foreach(iter, &PGrnScanOpaques)
	{
		PGrnScanOpaque so;

		so = slist_container(PGrnScanOpaqueData, node, iter.cur);
		score += PGrnCollectScoreScanOpaque(table, tuple, so);
	}

	return score;
}

/**
 * pgroonga.score(row record) : float8
 */
Datum
pgroonga_score(PG_FUNCTION_ARGS)
{
	HeapTupleHeader header = PG_GETARG_HEAPTUPLEHEADER(0);
	Oid	type;
	int32 recordType;
	TupleDesc desc;
	double score = 0.0;

	type = HeapTupleHeaderGetTypeId(header);
	recordType = HeapTupleHeaderGetTypMod(header);
	desc = lookup_rowtype_tupdesc(type, recordType);

	if (desc->natts > 0 && !slist_is_empty(&PGrnScanOpaques))
	{
		HeapTupleData tupleData;
		HeapTuple tuple;
		Relation table;

		tupleData.t_len = HeapTupleHeaderGetDatumLength(header);
		tupleData.t_tableOid = desc->attrs[0]->attrelid;
		tupleData.t_data = header;
		tuple = &tupleData;

		table = RelationIdGetRelation(tuple->t_tableOid);

		score = PGrnCollectScore(table, tuple);

		RelationClose(table);
	}

	ReleaseTupleDesc(desc);

	PG_RETURN_FLOAT8(score);
}

/**
 * pgroonga.table_name(indexName cstring) : cstring
 */
Datum
pgroonga_table_name(PG_FUNCTION_ARGS)
{
	Datum indexNameDatum = PG_GETARG_DATUM(0);
	Datum indexOidDatum;
	Oid indexOid;
	Oid fileNodeOid;
	char tableName[GRN_TABLE_MAX_KEY_SIZE];
	char *copiedTableName;

	indexOidDatum = DirectFunctionCall1(regclassin, indexNameDatum);
	if (!OidIsValid(indexOidDatum))
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_NAME),
				 errmsg("unknown index name: <%s>",
						DatumGetCString(indexNameDatum))));
	}
	indexOid = DatumGetObjectId(indexOidDatum);

	{
		HeapTuple tuple;
		tuple = SearchSysCache1(RELOID, indexOid);
		if (!HeapTupleIsValid(tuple)) {
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_NAME),
					 errmsg("failed to find file node ID from index name: <%s>",
							DatumGetCString(indexNameDatum))));
		}
		{
			Form_pg_class indexClass = (Form_pg_class) GETSTRUCT(tuple);
			fileNodeOid = indexClass->relfilenode;
			ReleaseSysCache(tuple);
		}
	}

	snprintf(tableName, sizeof(tableName),
			 PGrnSourcesTableNameFormat,
			 fileNodeOid);
	copiedTableName = pstrdup(tableName);
	PG_RETURN_CSTRING(copiedTableName);
}

/**
 * pgroonga.command(groongaCommand text) : text
 */
Datum
pgroonga_command(PG_FUNCTION_ARGS)
{
	text *groongaCommand = PG_GETARG_TEXT_PP(0);
	grn_rc rc;
	int flags = 0;
	text *result;

	grn_ctx_send(ctx,
				 VARDATA_ANY(groongaCommand),
				 VARSIZE_ANY_EXHDR(groongaCommand), 0);
	rc = ctx->rc;

	GRN_BULK_REWIND(&bodyBuffer);
	do {
		char *chunk;
		unsigned int chunkSize;
		grn_ctx_recv(ctx, &chunk, &chunkSize, &flags);
		GRN_TEXT_PUT(ctx, &bodyBuffer, chunk, chunkSize);
	} while ((flags & GRN_CTX_MORE));

	GRN_BULK_REWIND(&headBuffer);
	GRN_BULK_REWIND(&footBuffer);
	grn_output_envelope(ctx,
						rc,
						&headBuffer,
						&bodyBuffer,
						&footBuffer,
						NULL,
						0);

	grn_obj_reinit(ctx, &buffer, GRN_DB_TEXT, 0);
	GRN_TEXT_PUT(ctx, &buffer,
				 GRN_TEXT_VALUE(&headBuffer), GRN_TEXT_LEN(&headBuffer));
	GRN_TEXT_PUT(ctx, &buffer,
				 GRN_TEXT_VALUE(&bodyBuffer), GRN_TEXT_LEN(&bodyBuffer));
	GRN_TEXT_PUT(ctx, &buffer,
				 GRN_TEXT_VALUE(&footBuffer), GRN_TEXT_LEN(&footBuffer));
	result = cstring_to_text_with_len(GRN_TEXT_VALUE(&buffer),
									  GRN_TEXT_LEN(&buffer));
	PG_RETURN_TEXT_P(result);
}

static grn_obj *
PGrnSnipCreate(ArrayType *keywords)
{
	grn_obj *snip;
	int flags = GRN_SNIP_SKIP_LEADING_SPACES;
	unsigned int width = 200;
	unsigned int maxNResults = 3;
	const char *openTag = "<span class=\"keyword\">";
	const char *closeTag = "</span>";
	grn_snip_mapping *mapping = GRN_SNIP_MAPPING_HTML_ESCAPE;

	snip = grn_snip_open(ctx, flags, width, maxNResults,
						 openTag, strlen(openTag),
						 closeTag, strlen(closeTag),
						 mapping);
	if (!snip)
	{
		ereport(ERROR,
				(errcode(ERRCODE_OUT_OF_MEMORY),
				 errmsg("pgroonga: "
						"failed to allocate memory for generating snippet")));
		return NULL;
	}

	grn_snip_set_normalizer(ctx, snip, GRN_NORMALIZER_AUTO);

	{
		int i, n;

		n = ARR_DIMS(keywords)[0];
		for (i = 1; i <= n; i++)
		{
			Datum keywordDatum;
			text *keyword;
			bool isNULL;

			keywordDatum = array_ref(keywords, 1, &i, -1, -1, false,
									 'i', &isNULL);
			if (isNULL)
				continue;

			keyword = DatumGetTextPP(keywordDatum);
			grn_snip_add_cond(ctx, snip,
							  VARDATA_ANY(keyword),
							  VARSIZE_ANY_EXHDR(keyword),
							  NULL, 0, NULL, 0);
		}
	}

	return snip;
}

static grn_rc
PGrnSnipExec(grn_obj *snip, text *target, ArrayType **snippetArray)
{
	grn_rc rc;
	unsigned int i, nResults, maxTaggedLength;
	char *buffer;
	Datum *snippets;
	int	dims[1];
	int	lbs[1];

	rc = grn_snip_exec(ctx, snip,
					   VARDATA_ANY(target),
					   VARSIZE_ANY_EXHDR(target),
					   &nResults, &maxTaggedLength);
	if (rc != GRN_SUCCESS)
	{
		return rc;
	}

	if (nResults == 0)
	{
		*snippetArray = construct_empty_array(TEXTOID);
		return GRN_SUCCESS;
	}

	buffer = palloc(sizeof(char) * maxTaggedLength);
	snippets = palloc(sizeof(Datum) * nResults);
	for (i = 0; i < nResults; i++)
	{
		grn_rc rc;
		unsigned int snippetLength = 0;

		rc = grn_snip_get_result(ctx, snip, i, buffer, &snippetLength);
		if (rc != GRN_SUCCESS)
		{
			pfree(buffer);
			return rc;
		}
		snippets[i] = PointerGetDatum(cstring_to_text_with_len(buffer,
															   snippetLength));
    }
	pfree(buffer);

	dims[0] = nResults;
	lbs[0] = 1;

	*snippetArray = construct_md_array(snippets, NULL,
									   1, dims, lbs,
									   TEXTOID, -1, false, 'i');
	return GRN_SUCCESS;
}

/**
 * pgroonga.snippet_html(target text, keywords text[]) : text[]
 */
Datum
pgroonga_snippet_html(PG_FUNCTION_ARGS)
{
	text *target = PG_GETARG_TEXT_PP(0);
	ArrayType *keywords = PG_GETARG_ARRAYTYPE_P(1);
	grn_obj *snip;
	grn_rc rc;
	ArrayType *snippets;

	snip = PGrnSnipCreate(keywords);
	rc = PGrnSnipExec(snip, target, &snippets);
	grn_obj_close(ctx, snip);

	if (rc != GRN_SUCCESS) {
		ereport(ERROR,
				(errcode(PGrnRCToPgErrorCode(rc)),
				 errmsg("pgroonga: failed to compute snippets")));
	}

	PG_RETURN_POINTER(snippets);
}

static grn_bool
pgroonga_contain_raw(const char *text, unsigned int textSize,
					 const char *subText, unsigned int subTextSize)
{
	grn_bool contained;
	grn_obj targetBuffer;
	grn_obj subTextBuffer;

	GRN_TEXT_INIT(&targetBuffer, GRN_OBJ_DO_SHALLOW_COPY);
	GRN_TEXT_SET(ctx, &targetBuffer, text, textSize);

	GRN_TEXT_INIT(&subTextBuffer, GRN_OBJ_DO_SHALLOW_COPY);
	GRN_TEXT_SET(ctx, &subTextBuffer, subText, subTextSize);

	contained = grn_operator_exec_match(ctx, &targetBuffer, &subTextBuffer);

	GRN_OBJ_FIN(ctx, &targetBuffer);
	GRN_OBJ_FIN(ctx, &subTextBuffer);

	return contained;
}

/**
 * pgroonga.contain(target text, query text) : bool
 */
Datum
pgroonga_contain_text(PG_FUNCTION_ARGS)
{
	text *target = PG_GETARG_TEXT_PP(0);
	text *query = PG_GETARG_TEXT_PP(1);
	grn_bool contained;

	contained = pgroonga_contain_raw(VARDATA_ANY(target),
									 VARSIZE_ANY_EXHDR(target),
									 VARDATA_ANY(query),
									 VARSIZE_ANY_EXHDR(query));
	PG_RETURN_BOOL(contained);
}

/**
 * pgroonga.contain(target text[], query text) : bool
 */
Datum
pgroonga_contain_text_array(PG_FUNCTION_ARGS)
{
	ArrayType *target = PG_GETARG_ARRAYTYPE_P(0);
	text *query = PG_GETARG_TEXT_PP(1);
	bool contained = false;
	grn_obj elementBuffer;
	int i, n;

	grn_obj_reinit(ctx, &buffer, GRN_DB_TEXT, 0);
	GRN_TEXT_SET(ctx, &buffer, VARDATA_ANY(query), VARSIZE_ANY_EXHDR(query));

	GRN_TEXT_INIT(&elementBuffer, GRN_OBJ_DO_SHALLOW_COPY);

	n = ARR_DIMS(target)[0];
	for (i = 1; i <= n; i++)
	{
		Datum elementDatum;
		text *element;
		bool isNULL;

		elementDatum = array_ref(target, 1, &i, -1, -1, false, 'i', &isNULL);
		if (isNULL)
			continue;

		element = DatumGetTextPP(elementDatum);
		GRN_TEXT_SET(ctx, &elementBuffer,
					 VARDATA_ANY(element), VARSIZE_ANY_EXHDR(element));
		if (pgroonga_contain_raw(GRN_TEXT_VALUE(&elementBuffer),
								 GRN_TEXT_LEN(&elementBuffer),
								 GRN_TEXT_VALUE(&buffer),
								 GRN_TEXT_LEN(&buffer)))
		{
			contained = true;
			break;
		}
	}

	GRN_OBJ_FIN(ctx, &elementBuffer);

	PG_RETURN_BOOL(contained);
}

/**
 * pgroonga.contain(target varchar, query varchar) : bool
 */
Datum
pgroonga_contain_varchar(PG_FUNCTION_ARGS)
{
	VarChar *target = PG_GETARG_VARCHAR_PP(0);
	VarChar *query = PG_GETARG_VARCHAR_PP(1);
	grn_bool contained;

	contained =
		pgroonga_contain_raw(VARDATA_ANY(target), VARSIZE_ANY_EXHDR(target),
							 VARDATA_ANY(query), VARSIZE_ANY_EXHDR(query));
	PG_RETURN_BOOL(contained);
}

/**
 * pgroonga.contain(target varchar[], query varchar) : bool
 */
Datum
pgroonga_contain_varchar_array(PG_FUNCTION_ARGS)
{
	ArrayType *target = PG_GETARG_ARRAYTYPE_P(0);
	VarChar *query = PG_GETARG_VARCHAR_PP(1);
	bool contained = false;
	grn_obj elementBuffer;
	int i, n;

	grn_obj_reinit(ctx, &buffer, GRN_DB_TEXT, 0);
	GRN_TEXT_SET(ctx, &buffer, VARDATA_ANY(query), VARSIZE_ANY_EXHDR(query));

	GRN_TEXT_INIT(&elementBuffer, GRN_OBJ_DO_SHALLOW_COPY);

	n = ARR_DIMS(target)[0];
	for (i = 1; i <= n; i++)
	{
		Datum elementDatum;
		VarChar *element;
		bool isNULL;

		elementDatum = array_ref(target, 1, &i, -1, -1, false, 'i', &isNULL);
		if (isNULL)
			continue;

		element = DatumGetVarCharPP(elementDatum);
		GRN_TEXT_SET(ctx, &elementBuffer,
					 VARDATA_ANY(element), VARSIZE_ANY_EXHDR(element));
		if (grn_operator_exec_equal(ctx, &buffer, &elementBuffer))
		{
			contained = true;
			break;
		}
	}

	GRN_OBJ_FIN(ctx, &elementBuffer);

	PG_RETURN_BOOL(contained);
}

/**
 * pgroonga.match(text, query) : bool
 */
Datum
pgroonga_match(PG_FUNCTION_ARGS)
{
#ifdef NOT_USED
	text *text = PG_GETARG_TEXT_PP(0);
	text *query = PG_GETARG_TEXT_PP(1);
#endif

	ereport(ERROR,
			(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
			 errmsg("pgroonga: operator @@ is available only in index scans")));

	PG_RETURN_BOOL(false);
}

static void
PGrnInsert(Relation index,
		   grn_obj *sourcesTable,
		   grn_obj *sourcesCtidColumn,
		   Datum *values,
		   bool *isnull,
		   ItemPointer ht_ctid)
{
	TupleDesc desc = RelationGetDescr(index);
	grn_id id;
	int i;

	id = grn_table_add(ctx, sourcesTable, NULL, 0, NULL);
	GRN_UINT64_SET(ctx, &ctidBuffer, CtidToUInt64(ht_ctid));
	grn_obj_set_value(ctx, sourcesCtidColumn, id, &ctidBuffer, GRN_OBJ_SET);

	for (i = 0; i < desc->natts; i++)
	{
		grn_obj *dataColumn;
		Form_pg_attribute attribute = desc->attrs[i];
		NameData *name;
		grn_id domain;
		unsigned char flags;

		name = &(attribute->attname);
		if (isnull[i])
			continue;

		dataColumn = grn_obj_column(ctx, sourcesTable,
									name->data, strlen(name->data));
		domain = PGrnGetType(index, i, &flags);
		grn_obj_reinit(ctx, &buffer, domain, flags);
		PGrnConvertDatum(values[i], attribute->atttypid, &buffer);
		grn_obj_set_value(ctx, dataColumn, id, &buffer, GRN_OBJ_SET);
		grn_obj_unlink(ctx, dataColumn);
		if (!PGrnCheck("pgroonga: failed to set column value")) {
			continue;
		}
	}
}

/**
 * pgroonga.insert() -- aminsert
 */
Datum
pgroonga_insert(PG_FUNCTION_ARGS)
{
	Relation index = (Relation) PG_GETARG_POINTER(0);
	Datum *values = (Datum *) PG_GETARG_POINTER(1);
	bool *isnull = (bool *) PG_GETARG_POINTER(2);
	ItemPointer ht_ctid = (ItemPointer) PG_GETARG_POINTER(3);
#ifdef NOT_USED
	Relation heap = (Relation) PG_GETARG_POINTER(4);
	IndexUniqueCheck checkUnique = PG_GETARG_INT32(5);
#endif
	grn_obj *sourcesTable;
	grn_obj *sourcesCtidColumn;

	sourcesTable = PGrnLookupSourcesTable(index, ERROR);
	sourcesCtidColumn = PGrnLookupSourcesCtidColumn(index, ERROR);
	PGrnInsert(index, sourcesTable, sourcesCtidColumn,
			   values, isnull, ht_ctid);
	grn_db_touch(ctx, grn_ctx_db(ctx));

	PG_RETURN_BOOL(true);
}

static void
PGrnFindPrimaryKey(Relation table,
				   AttrNumber *primaryKeyNumber,
				   Oid *primaryKeyTypeID)
{
	List *indexOIDList;
	ListCell *cell;

	*primaryKeyNumber = InvalidAttrNumber;
	*primaryKeyTypeID = InvalidOid;

	indexOIDList = RelationGetIndexList(table);
	if (indexOIDList == NIL)
	{
		return;
	}

	foreach(cell, indexOIDList)
	{
		Oid indexOID = lfirst_oid(cell);
		Relation index;

		index = index_open(indexOID, NoLock);
		if (!index->rd_index->indisprimary) {
			index_close(index, NoLock);
			continue;
		}

		if (index->rd_index->indnatts == 1)
		{
			TupleDesc desc;

			*primaryKeyNumber = index->rd_index->indkey.values[0];

			desc = RelationGetDescr(table);
			*primaryKeyTypeID = desc->attrs[*primaryKeyNumber - 1]->atttypid;
		}
		index_close(index, NoLock);

		break;
	}

	list_free(indexOIDList);
}

static void
PGrnScanOpaqueInitPrimaryKey(PGrnScanOpaque so, Relation index)
{
	Relation table;
	AttrNumber primaryKeyNumber = InvalidAttrNumber;
	Oid primaryKeyTypeID = InvalidOid;
	bool havePrimaryKeyInIndex = false;

	table = RelationIdGetRelation(so->dataTableID);
	PGrnFindPrimaryKey(table, &primaryKeyNumber, &primaryKeyTypeID);

	if (AttributeNumberIsValid(primaryKeyNumber))
	{
		int i, nColumns;

		nColumns = index->rd_index->indkey.ndim;
		for (i = 0; i < nColumns; i++)
		{
			if (index->rd_index->indkey.values[i] == primaryKeyNumber)
			{
				havePrimaryKeyInIndex = true;
				break;
			}
		}
	}

	if (havePrimaryKeyInIndex)
	{
		so->primaryKey.number = primaryKeyNumber;
		so->primaryKey.type = primaryKeyTypeID;
		so->primaryKey.domain = PGrnGetType(index,
											primaryKeyNumber,
											&(so->primaryKey.flags));
		so->primaryKey.indexColumn = PGrnLookupIndexColumn(index,
														   primaryKeyNumber - 1,
														   ERROR);
		so->primaryKey.lexicon =
			grn_ctx_at(ctx, so->primaryKey.indexColumn->header.domain);
	}
	else
	{
		so->primaryKey.number = InvalidAttrNumber;
		so->primaryKey.type = InvalidOid;
		so->primaryKey.domain = GRN_ID_NIL;
		so->primaryKey.flags = 0;
		so->primaryKey.lexicon = NULL;
		so->primaryKey.indexColumn = NULL;
	}

	RelationClose(table);
}

static void
PGrnScanOpaqueInit(PGrnScanOpaque so, Relation index)
{
	so->dataTableID = index->rd_index->indrelid;
	PGrnScanOpaqueInitPrimaryKey(so, index);
	so->sourcesTable = PGrnLookupSourcesTable(index, ERROR);
	so->sourcesCtidColumn = PGrnLookupSourcesCtidColumn(index, ERROR);
	GRN_VOID_INIT(&(so->minBorderValue));
	GRN_VOID_INIT(&(so->maxBorderValue));
	so->searched = NULL;
	so->sorted = NULL;
	so->targetTable = NULL;
	so->indexCursor = NULL;
	so->tableCursor = NULL;
	so->ctidAccessor = NULL;
	so->scoreAccessor = NULL;
	so->currentID = GRN_ID_NIL;

	slist_push_head(&PGrnScanOpaques, &(so->node));
}

static void
PGrnScanOpaqueReinit(PGrnScanOpaque so)
{
	so->currentID = GRN_ID_NIL;
	if (so->scoreAccessor)
	{
		grn_obj_unlink(ctx, so->scoreAccessor);
		so->scoreAccessor = NULL;
	}
	if (so->ctidAccessor)
	{
		grn_obj_unlink(ctx, so->ctidAccessor);
		so->ctidAccessor = NULL;
	}
	if (so->indexCursor)
	{
		grn_obj_close(ctx, so->indexCursor);
		so->indexCursor = NULL;
	}
	if (so->tableCursor)
	{
		grn_table_cursor_close(ctx, so->tableCursor);
		so->tableCursor = NULL;
	}
	GRN_OBJ_FIN(ctx, &(so->minBorderValue));
	GRN_OBJ_FIN(ctx, &(so->maxBorderValue));
	if (so->sorted)
	{
		grn_obj_unlink(ctx, so->sorted);
		so->sorted = NULL;
	}
	if (so->searched)
	{
		grn_obj_unlink(ctx, so->searched);
		so->searched = NULL;
	}
}

static void
PGrnScanOpaqueFin(PGrnScanOpaque so)
{
	slist_mutable_iter iter;

	slist_foreach_modify(iter, &PGrnScanOpaques)
	{
		PGrnScanOpaque currentSo;

		currentSo = slist_container(PGrnScanOpaqueData, node, iter.cur);
		if (currentSo == so)
		{
			slist_delete_current(&iter);
			break;
		}
	}

	PGrnScanOpaqueReinit(so);
}

/**
 * pgroonga.beginscan() -- ambeginscan
 */
Datum
pgroonga_beginscan(PG_FUNCTION_ARGS)
{
	Relation index = (Relation) PG_GETARG_POINTER(0);
	int nkeys = PG_GETARG_INT32(1);
	int norderbys = PG_GETARG_INT32(2);
	IndexScanDesc scan;
	PGrnScanOpaque so;

	scan = RelationGetIndexScan(index, nkeys, norderbys);

	so = (PGrnScanOpaque) palloc(sizeof(PGrnScanOpaqueData));
	PGrnScanOpaqueInit(so, index);

	scan->opaque = so;

	PG_RETURN_POINTER(scan);
}

static void
PGrnSearchBuildConditionLike(PGrnSearchData *data,
							 grn_obj *matchTarget,
							 grn_obj *query)
{
	grn_obj *expression;
	const char *queryRaw;
	size_t querySize;

	expression = data->expression;
	queryRaw = GRN_TEXT_VALUE(query);
	querySize = GRN_TEXT_LEN(query);

	if (querySize == 0)
	{
		data->isEmptyCondition = true;
		return;
	}

	if (!(queryRaw[0] == '%' && queryRaw[querySize - 1] == '%'))
	{
		data->isEmptyCondition = true;
		return;
	}

	grn_expr_append_obj(ctx, expression, matchTarget, GRN_OP_PUSH, 1);
	grn_expr_append_const_str(ctx, expression,
							  queryRaw + 1, querySize - 2,
							  GRN_OP_PUSH, 1);
	grn_expr_append_op(ctx, expression, GRN_OP_MATCH, 2);
}

static void
PGrnSearchBuildConditions(IndexScanDesc scan,
						  PGrnScanOpaque so,
						  PGrnSearchData *data)
{
	Relation index = scan->indexRelation;
	TupleDesc desc;
	int i, nExpressions = 0;

	desc = RelationGetDescr(index);
	for (i = 0; i < scan->numberOfKeys; i++)
	{
		ScanKey key = &(scan->keyData[i]);
		Form_pg_attribute attribute;
		grn_bool isValidStrategy = GRN_TRUE;
		const char *targetColumnName;
		grn_obj *targetColumn;
		grn_obj *matchTarget, *matchTargetVariable;
		grn_operator operator = GRN_OP_NOP;

		/* NULL key is not supported */
		if (key->sk_flags & SK_ISNULL)
			continue;

		attribute = desc->attrs[key->sk_attno - 1];

		GRN_EXPR_CREATE_FOR_QUERY(ctx, so->sourcesTable,
								  matchTarget, matchTargetVariable);
		GRN_PTR_PUT(ctx, &(data->matchTargets), matchTarget);

		targetColumnName = attribute->attname.data;
		targetColumn = grn_obj_column(ctx, so->sourcesTable,
									  targetColumnName,
									  strlen(targetColumnName));
		GRN_PTR_PUT(ctx, &(data->targetColumns), targetColumn);
		grn_expr_append_obj(ctx, matchTarget, targetColumn, GRN_OP_PUSH, 1);

		{
			grn_id domain;
			unsigned char flags = 0;
			domain = PGrnGetType(index, key->sk_attno - 1, NULL);
			grn_obj_reinit(ctx, &buffer, domain, flags);
		}
		{
			Oid valueTypeID = attribute->atttypid;
			switch (valueTypeID)
			{
			case VARCHARARRAYOID:
				valueTypeID = VARCHAROID;
				break;
			case TEXTARRAYOID:
				valueTypeID = TEXTOID;
				break;
			}
			PGrnConvertDatum(key->sk_argument, valueTypeID, &buffer);
		}

		switch (key->sk_strategy)
		{
		case PGrnLessStrategyNumber:
			operator = GRN_OP_LESS;
			break;
		case PGrnLessEqualStrategyNumber:
			operator = GRN_OP_LESS_EQUAL;
			break;
		case PGrnEqualStrategyNumber:
			operator = GRN_OP_EQUAL;
			break;
		case PGrnGreaterEqualStrategyNumber:
			operator = GRN_OP_GREATER_EQUAL;
			break;
		case PGrnGreaterStrategyNumber:
			operator = GRN_OP_GREATER;
			break;
		case PGrnLikeStrategyNumber:
			break;
		case PGrnContainStrategyNumber:
			operator = GRN_OP_MATCH;
			break;
		case PGrnQueryStrategyNumber:
			break;
		default:
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("unexpected strategy number: %d",
							key->sk_strategy)));
			isValidStrategy = GRN_FALSE;
			break;
		}

		if (!isValidStrategy)
			continue;

		switch (key->sk_strategy)
		{
		case PGrnLikeStrategyNumber:
			PGrnSearchBuildConditionLike(data, matchTarget, &buffer);
			if (data->isEmptyCondition)
				return;
			break;
		case PGrnQueryStrategyNumber:
		{
			grn_rc rc;
			grn_expr_flags flags =
				GRN_EXPR_SYNTAX_QUERY | GRN_EXPR_ALLOW_LEADING_NOT;
			rc = grn_expr_parse(ctx, data->expression,
								GRN_TEXT_VALUE(&buffer), GRN_TEXT_LEN(&buffer),
								matchTarget, GRN_OP_MATCH, GRN_OP_AND,
								flags);
			if (rc != GRN_SUCCESS)
			{
				ereport(ERROR,
						(errcode(PGrnRCToPgErrorCode(rc)),
						 errmsg("pgroonga: failed to parse expression: %s",
								ctx->errbuf)));
			}
			break;
		}
		default:
			grn_expr_append_obj(ctx, data->expression,
								matchTarget, GRN_OP_PUSH, 1);
			grn_expr_append_const(ctx, data->expression,
								  &buffer, GRN_OP_PUSH, 1);
			grn_expr_append_op(ctx, data->expression, operator, 2);
			break;
		}

		if (nExpressions > 0)
			grn_expr_append_op(ctx, data->expression, GRN_OP_AND, 2);
		nExpressions++;
	}
}

static void
PGrnSearchDataFree(PGrnSearchData *data)
{
	unsigned int i;
	unsigned int nMatchTargets;
	unsigned int nTargetColumns;

	grn_obj_unlink(ctx, data->expression);

	nMatchTargets = GRN_BULK_VSIZE(&(data->matchTargets)) / sizeof(grn_obj *);
	for (i = 0; i < nMatchTargets; i++)
	{
		grn_obj *matchTarget = GRN_PTR_VALUE_AT(&(data->matchTargets), i);
		grn_obj_unlink(ctx, matchTarget);
	}
	GRN_OBJ_FIN(ctx, &(data->matchTargets));

	nTargetColumns = GRN_BULK_VSIZE(&(data->targetColumns)) / sizeof(grn_obj *);
	for (i = 0; i < nTargetColumns; i++)
	{
		grn_obj *targetColumn = GRN_PTR_VALUE_AT(&(data->targetColumns), i);
		grn_obj_unlink(ctx, targetColumn);
	}
	GRN_OBJ_FIN(ctx, &(data->targetColumns));

	GRN_OBJ_FIN(ctx, &(data->sectionID));
}

static void
PGrnSearch(IndexScanDesc scan)
{
	PGrnScanOpaque so = (PGrnScanOpaque) scan->opaque;
	PGrnSearchData data;

	if (scan->numberOfKeys == 0)
		return;

	GRN_PTR_INIT(&(data.matchTargets), GRN_OBJ_VECTOR, GRN_ID_NIL);
	GRN_PTR_INIT(&(data.targetColumns), GRN_OBJ_VECTOR, GRN_ID_NIL);
	GRN_UINT32_INIT(&(data.sectionID), 0);

	GRN_EXPR_CREATE_FOR_QUERY(ctx, so->sourcesTable,
							  data.expression, data.expressionVariable);
	data.isEmptyCondition = false;

	PG_TRY();
	{
		PGrnSearchBuildConditions(scan, so, &data);
	}
	PG_CATCH();
	{
		PGrnSearchDataFree(&data);
		PG_RE_THROW();
	}
	PG_END_TRY();

	/* TODO: Add NULL check for so->searched. */
	so->searched = grn_table_create(ctx, NULL, 0, NULL,
									GRN_OBJ_TABLE_HASH_KEY | GRN_OBJ_WITH_SUBREC,
									so->sourcesTable, 0);
	if (!data.isEmptyCondition)
	{
		grn_table_select(ctx,
						 so->sourcesTable,
						 data.expression,
						 so->searched,
						 GRN_OP_OR);
	}
	PGrnSearchDataFree(&data);
}

static void
PGrnSort(IndexScanDesc scan)
{
	/* TODO */
}

static void
PGrnOpenTableCursor(IndexScanDesc scan, ScanDirection dir)
{
	PGrnScanOpaque so = (PGrnScanOpaque) scan->opaque;
	grn_obj *table;
	int offset = 0;
	int limit = -1;
	int flags = 0;

	table = so->sorted;
	if (!table)
		table = so->searched;
	if (!table)
		table = so->sourcesTable;

	if (dir == BackwardScanDirection)
		flags |= GRN_CURSOR_DESCENDING;
	else
		flags |= GRN_CURSOR_ASCENDING;

	so->tableCursor = grn_table_cursor_open(ctx, table,
											NULL, 0, NULL, 0,
											offset, limit, flags);
	so->ctidAccessor = grn_obj_column(ctx, table,
									  PGrnSourcesCtidColumnName,
									  PGrnSourcesCtidColumnNameLength);
	if (so->searched)
	{
		so->scoreAccessor = grn_obj_column(ctx, so->searched,
										   GRN_COLUMN_NAME_SCORE,
										   GRN_COLUMN_NAME_SCORE_LEN);
	}
}

static bool
PGrnIsMeaningfullMaxBorderValue(grn_obj *currentValue,
								grn_obj *newValue,
								int flags,
								StrategyNumber strategy)
{
	if (((flags & GRN_CURSOR_LT) == GRN_CURSOR_LT) &&
		strategy == PGrnLessEqualStrategyNumber)
	{
		return grn_operator_exec_greater_equal(ctx, currentValue, newValue);
	}
	else
	{
		return grn_operator_exec_greater(ctx, currentValue, newValue);
	}
}

static bool
PGrnIsMeaningfullMinBorderValue(grn_obj *currentValue,
								grn_obj *newValue,
								int flags,
								StrategyNumber strategy)
{
	if (((flags & GRN_CURSOR_GT) == GRN_CURSOR_GT) &&
		strategy == PGrnGreaterEqualStrategyNumber)
	{
		return grn_operator_exec_less_equal(ctx, currentValue, newValue);
	}
	else
	{
		return grn_operator_exec_less(ctx, currentValue, newValue);
	}
}

static void
PGrnFillBorder(IndexScanDesc scan,
			   void **min, unsigned int *minSize,
			   void **max, unsigned int *maxSize,
			   int *flags)
{
	Relation index = scan->indexRelation;
	TupleDesc desc;
	PGrnScanOpaque so = (PGrnScanOpaque) scan->opaque;
	grn_obj *minBorderValue;
	grn_obj *maxBorderValue;
	int i;

	desc = RelationGetDescr(index);

	minBorderValue = &(so->minBorderValue);
	maxBorderValue = &(so->maxBorderValue);
	for (i = 0; i < scan->numberOfKeys; i++)
	{
		ScanKey key = &(scan->keyData[i]);
		AttrNumber attrNumber;
		Form_pg_attribute attribute;
		grn_id domain;

		attrNumber = key->sk_attno - 1;
		attribute = desc->attrs[attrNumber];

		domain = PGrnGetType(index, attrNumber, NULL);
		switch (key->sk_strategy)
		{
		case PGrnLessStrategyNumber:
		case PGrnLessEqualStrategyNumber:
			if (maxBorderValue->header.type != GRN_DB_VOID)
			{
				grn_obj_reinit(ctx, &buffer, domain, 0);
				PGrnConvertDatum(key->sk_argument,
								 attribute->atttypid,
								 &buffer);
				if (!PGrnIsMeaningfullMaxBorderValue(maxBorderValue,
													 &buffer,
													 *flags,
													 key->sk_strategy))
				{
					continue;
				}
			}
			grn_obj_reinit(ctx, maxBorderValue, domain, 0);
			PGrnConvertDatum(key->sk_argument,
							 attribute->atttypid,
							 maxBorderValue);
			*max = GRN_BULK_HEAD(maxBorderValue);
			*maxSize = GRN_BULK_VSIZE(maxBorderValue);
			*flags &= ~(GRN_CURSOR_LT | GRN_CURSOR_LE);
			if (key->sk_strategy == PGrnLessStrategyNumber)
			{
				*flags |= GRN_CURSOR_LT;
			}
			else
			{
				*flags |= GRN_CURSOR_LE;
			}
			break;
		case PGrnGreaterEqualStrategyNumber:
		case PGrnGreaterStrategyNumber:
			if (minBorderValue->header.type != GRN_DB_VOID)
			{
				grn_obj_reinit(ctx, &buffer, domain, 0);
				PGrnConvertDatum(key->sk_argument,
								 attribute->atttypid,
								 &buffer);
				if (!PGrnIsMeaningfullMinBorderValue(minBorderValue,
													 &buffer,
													 *flags,
													 key->sk_strategy))
				{
					continue;
				}
			}
			grn_obj_reinit(ctx, minBorderValue, domain, 0);
			PGrnConvertDatum(key->sk_argument,
							 attribute->atttypid,
							 minBorderValue);
			*min = GRN_BULK_HEAD(minBorderValue);
			*minSize = GRN_BULK_VSIZE(minBorderValue);
			*flags &= ~(GRN_CURSOR_GT | GRN_CURSOR_GE);
			if (key->sk_strategy == PGrnGreaterEqualStrategyNumber)
			{
				*flags |= GRN_CURSOR_GE;
			}
			else
			{
				*flags |= GRN_CURSOR_GT;
			}
			break;
		default:
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("unexpected strategy number for range search: %d",
							key->sk_strategy)));
			break;
		}
	}
}

static void
PGrnRangeSearch(IndexScanDesc scan, ScanDirection dir)
{
	PGrnScanOpaque so = (PGrnScanOpaque) scan->opaque;
	void *min = NULL;
	unsigned int minSize = 0;
	void *max = NULL;
	unsigned int maxSize = 0;
	int offset = 0;
	int limit = -1;
	int flags = 0;
	grn_id indexCursorMin = GRN_ID_NIL;
	grn_id indexCursorMax = GRN_ID_MAX;
	int indexCursorFlags = 0;
	grn_obj *indexColumn;
	grn_obj *lexicon;
	int i;
	unsigned int nthAttribute = 0;

	PGrnFillBorder(scan, &min, &minSize, &max, &maxSize, &flags);

	if (dir == BackwardScanDirection)
		flags |= GRN_CURSOR_DESCENDING;
	else
		flags |= GRN_CURSOR_ASCENDING;

	for (i = 0; i < scan->numberOfKeys; i++)
	{
		ScanKey key = &(scan->keyData[i]);
		nthAttribute = key->sk_attno - 1;
		break;
	}
	indexColumn = PGrnLookupIndexColumn(scan->indexRelation, nthAttribute,
										ERROR);
	lexicon = grn_column_table(ctx, indexColumn);

	so->tableCursor = grn_table_cursor_open(ctx, lexicon,
											min, minSize,
											max, maxSize,
											offset, limit, flags);
	so->indexCursor = grn_index_cursor_open(ctx,
											so->tableCursor, indexColumn,
											indexCursorMin,
											indexCursorMax,
											indexCursorFlags);
	so->ctidAccessor = grn_obj_column(ctx, so->sourcesTable,
									  PGrnSourcesCtidColumnName,
									  PGrnSourcesCtidColumnNameLength);
}

static bool
PGrnIsRangeSearchable(IndexScanDesc scan)
{
	int i;
	AttrNumber previousAttrNumber = InvalidAttrNumber;

	for (i = 0; i < scan->numberOfKeys; i++)
	{
		ScanKey key = &(scan->keyData[i]);

		if (previousAttrNumber == InvalidAttrNumber)
		{
			previousAttrNumber = key->sk_attno;
		}
		if (key->sk_attno != previousAttrNumber)
		{
			return false;
		}

		switch (key->sk_strategy)
		{
		case PGrnLessStrategyNumber:
		case PGrnLessEqualStrategyNumber:
		case PGrnGreaterStrategyNumber:
		case PGrnGreaterEqualStrategyNumber:
			break;
		default:
			return false;
			break;
		}
	}

	return true;
}

static void
PGrnEnsureCursorOpened(IndexScanDesc scan, ScanDirection dir)
{
	PGrnScanOpaque so = (PGrnScanOpaque) scan->opaque;

	if (so->indexCursor)
		return;
	if (so->tableCursor)
		return;

	if (PGrnIsRangeSearchable(scan))
	{
		PGrnRangeSearch(scan, dir);
	}
	else
	{
		PGrnSearch(scan);
		PGrnSort(scan);
		PGrnOpenTableCursor(scan, dir);
	}
}


/**
 * pgroonga.gettuple() -- amgettuple
 */
Datum
pgroonga_gettuple(PG_FUNCTION_ARGS)
{
	IndexScanDesc scan = (IndexScanDesc) PG_GETARG_POINTER(0);
	ScanDirection dir = (ScanDirection) PG_GETARG_INT32(1);
	PGrnScanOpaque so = (PGrnScanOpaque) scan->opaque;

	scan->xs_recheck = false;

	PGrnEnsureCursorOpened(scan, dir);

	if (scan->kill_prior_tuple && so->currentID != GRN_ID_NIL)
	{
		grn_table_delete_by_id(ctx, so->sourcesTable, so->currentID);
	}

	if (so->indexCursor)
	{
		grn_posting *posting;
		grn_id termID;
		grn_id id = GRN_ID_NIL;
		posting = grn_index_cursor_next(ctx, so->indexCursor, &termID);
		if (posting)
			id = posting->rid;
		so->currentID = id;
	}
	else
	{
		so->currentID = grn_table_cursor_next(ctx, so->tableCursor);
	}

	if (so->currentID == GRN_ID_NIL)
	{
		PG_RETURN_BOOL(false);
	}
	else
	{
		GRN_BULK_REWIND(&ctidBuffer);
		grn_obj_get_value(ctx, so->ctidAccessor, so->currentID, &ctidBuffer);
		scan->xs_ctup.t_self = UInt64ToCtid(GRN_UINT64_VALUE(&ctidBuffer));

		PG_RETURN_BOOL(true);
	}

}

/**
 * pgroonga.getbitmap() -- amgetbitmap
 */
Datum
pgroonga_getbitmap(PG_FUNCTION_ARGS)
{
	IndexScanDesc scan = (IndexScanDesc) PG_GETARG_POINTER(0);
	TIDBitmap *tbm = (TIDBitmap *) PG_GETARG_POINTER(1);
	PGrnScanOpaque so = (PGrnScanOpaque) scan->opaque;
	int64 nRecords = 0;

	PGrnEnsureCursorOpened(scan, ForwardScanDirection);

	if (so->indexCursor)
	{
		grn_posting *posting;
		grn_id termID;
		while ((posting = grn_index_cursor_next(ctx, so->indexCursor, &termID)))
		{
			ItemPointerData ctid;
			GRN_BULK_REWIND(&ctidBuffer);
			grn_obj_get_value(ctx, so->ctidAccessor, posting->rid, &ctidBuffer);
			ctid = UInt64ToCtid(GRN_UINT64_VALUE(&ctidBuffer));
			tbm_add_tuples(tbm, &ctid, 1, false);
			nRecords++;
		}
	}
	else
	{
		grn_id id;
		while ((id = grn_table_cursor_next(ctx, so->tableCursor)) != GRN_ID_NIL)
		{
			ItemPointerData ctid;
			GRN_BULK_REWIND(&ctidBuffer);
			grn_obj_get_value(ctx, so->ctidAccessor, id, &ctidBuffer);
			ctid = UInt64ToCtid(GRN_UINT64_VALUE(&ctidBuffer));
			tbm_add_tuples(tbm, &ctid, 1, false);
			nRecords++;
		}
	}

	PG_RETURN_INT64(nRecords);
}

/**
 * pgroonga.rescan() -- amrescan
 */
Datum
pgroonga_rescan(PG_FUNCTION_ARGS)
{
	IndexScanDesc scan = (IndexScanDesc) PG_GETARG_POINTER(0);
	ScanKey	keys = (ScanKey) PG_GETARG_POINTER(1);
#ifdef NOT_USED
	int nkeys = PG_GETARG_INT32(2);
	ScanKey	orderbys = (ScanKey) PG_GETARG_POINTER(3);
	int norderbys = PG_GETARG_INT32(4);
#endif
	PGrnScanOpaque so = (PGrnScanOpaque) scan->opaque;

	PGrnScanOpaqueReinit(so);

	if (keys && scan->numberOfKeys > 0)
		memmove(scan->keyData, keys, scan->numberOfKeys * sizeof(ScanKeyData));

	PG_RETURN_VOID();
}

/**
 * pgroonga.endscan() -- amendscan
 */
Datum
pgroonga_endscan(PG_FUNCTION_ARGS)
{
	IndexScanDesc scan = (IndexScanDesc) PG_GETARG_POINTER(0);
	PGrnScanOpaque so = (PGrnScanOpaque) scan->opaque;

	PGrnScanOpaqueFin(so);
	pfree(so);

	PG_RETURN_VOID();
}

static void
PGrnBuildCallback(Relation index,
				  HeapTuple htup,
				  Datum *values,
				  bool *isnull,
				  bool tupleIsAlive,
				  void *state)
{
	PGrnBuildState bs = (PGrnBuildState) state;

	if (tupleIsAlive) {
		PGrnInsert(index, bs->sourcesTable, bs->sourcesCtidColumn,
				   values, isnull, &(htup->t_self));
		bs->nIndexedTuples++;
	}
}

/**
 * pgroonga.build() -- ambuild
 */
Datum
pgroonga_build(PG_FUNCTION_ARGS)
{
	Relation heap = (Relation) PG_GETARG_POINTER(0);
	Relation index = (Relation) PG_GETARG_POINTER(1);
	IndexInfo *indexInfo = (IndexInfo *) PG_GETARG_POINTER(2);
	IndexBuildResult *result;
	double nHeapTuples = 0.0;
	PGrnBuildStateData bs;
	grn_obj lexicons;

	if (indexInfo->ii_Unique)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("pgroonga: unique index isn't supported")));

	bs.sourcesTable = NULL;
	bs.nIndexedTuples = 0.0;

	GRN_PTR_INIT(&lexicons, GRN_OBJ_VECTOR, GRN_ID_NIL);
	PG_TRY();
	{
		PGrnCreate(index,
				   &(bs.sourcesTable),
				   &(bs.sourcesCtidColumn),
				   &lexicons);
		nHeapTuples = IndexBuildHeapScan(heap, index, indexInfo, true,
										 PGrnBuildCallback, &bs);
		PGrnSetSources(index, bs.sourcesTable);
	}
	PG_CATCH();
	{
		size_t i, nLexicons;

		nLexicons = GRN_BULK_VSIZE(&lexicons) / sizeof(grn_obj *);
		for (i = 0; i < nLexicons; i++)
		{
			grn_obj *lexicon = GRN_PTR_VALUE_AT(&lexicons, i);
			grn_obj_remove(ctx, lexicon);
		}
		GRN_OBJ_FIN(ctx, &lexicons);

		if (bs.sourcesTable)
			grn_obj_remove(ctx, bs.sourcesTable);

		PG_RE_THROW();
	}
	PG_END_TRY();
	GRN_OBJ_FIN(ctx, &lexicons);

	result = (IndexBuildResult *) palloc(sizeof(IndexBuildResult));
	result->heap_tuples = nHeapTuples;
	result->index_tuples = bs.nIndexedTuples;

	PG_RETURN_POINTER(result);
}

/**
 * pgroonga.buildempty() -- ambuildempty
 */
Datum
pgroonga_buildempty(PG_FUNCTION_ARGS)
{
	Relation index = (Relation) PG_GETARG_POINTER(0);
	grn_obj *sourcesTable = NULL;
	grn_obj *sourcesCtidColumn = NULL;
	grn_obj lexicons;

	GRN_PTR_INIT(&lexicons, GRN_OBJ_VECTOR, GRN_ID_NIL);
	PG_TRY();
	{
		PGrnCreate(index, &sourcesTable, &sourcesCtidColumn, &lexicons);
		PGrnSetSources(index, sourcesTable);
	}
	PG_CATCH();
	{
		size_t i, nLexicons;

		nLexicons = GRN_BULK_VSIZE(&lexicons) / sizeof(grn_obj *);
		for (i = 0; i < nLexicons; i++)
		{
			grn_obj *lexicon = GRN_PTR_VALUE_AT(&lexicons, i);
			grn_obj_remove(ctx, lexicon);
		}
		GRN_OBJ_FIN(ctx, &lexicons);

		if (sourcesTable)
			grn_obj_remove(ctx, sourcesTable);

		PG_RE_THROW();
	}
	PG_END_TRY();
	GRN_OBJ_FIN(ctx, &lexicons);

	PG_RETURN_VOID();
}

static IndexBulkDeleteResult *
PGrnBulkDeleteResult(IndexVacuumInfo *info, grn_obj *sourcesTable)
{
	IndexBulkDeleteResult *stats;

	stats = (IndexBulkDeleteResult *) palloc0(sizeof(IndexBulkDeleteResult));
	stats->num_pages = (BlockNumber) 1;	/* TODO: sizeof index / BLCKSZ */

	/* table might be NULL if index is corrupted */
	if (sourcesTable)
		stats->num_index_tuples = grn_table_size(ctx, sourcesTable);
	else
		stats->num_index_tuples = 0;

	return stats;
}

/**
 * pgroonga.bulkdelete() -- ambulkdelete
 */
Datum
pgroonga_bulkdelete(PG_FUNCTION_ARGS)
{
	IndexVacuumInfo *info = (IndexVacuumInfo *) PG_GETARG_POINTER(0);
	IndexBulkDeleteResult *stats = (IndexBulkDeleteResult *) PG_GETARG_POINTER(1);
	IndexBulkDeleteCallback	callback = (IndexBulkDeleteCallback) PG_GETARG_POINTER(2);
	void *callback_state = (void *) PG_GETARG_POINTER(3);
	Relation index = info->index;
	grn_obj	*sourcesTable;
	grn_table_cursor *cursor;
	double nRemovedTuples;

	sourcesTable = PGrnLookupSourcesTable(index, WARNING);

	if (!stats)
		stats = PGrnBulkDeleteResult(info, sourcesTable);

	if (!sourcesTable || !callback)
		PG_RETURN_POINTER(stats);

	nRemovedTuples = 0;

	cursor = grn_table_cursor_open(ctx, sourcesTable,
								   NULL, 0, NULL, 0,
								   0, -1, 0);
	if (!cursor)
		ereport(ERROR,
				(errcode(ERRCODE_SYSTEM_ERROR),
				 errmsg("pgroonga: failed to open cursor: %s", ctx->errbuf)));

	PG_TRY();
	{
		grn_id id;
		grn_obj *sourcesCtidColumn;

		sourcesCtidColumn = PGrnLookupSourcesCtidColumn(index, ERROR);
		while ((id = grn_table_cursor_next(ctx, cursor)) != GRN_ID_NIL)
		{
			ItemPointerData	ctid;

			CHECK_FOR_INTERRUPTS();

			GRN_BULK_REWIND(&ctidBuffer);
			grn_obj_get_value(ctx, sourcesCtidColumn, id, &ctidBuffer);
			ctid = UInt64ToCtid(GRN_UINT64_VALUE(&ctidBuffer));
			if (callback(&ctid, callback_state))
			{
				grn_table_cursor_delete(ctx, cursor);

				nRemovedTuples += 1;
			}
		}
		grn_table_cursor_close(ctx, cursor);
	}
	PG_CATCH();
	{
		grn_table_cursor_close(ctx, cursor);
		PG_RE_THROW();
	}
	PG_END_TRY();

	stats->tuples_removed = nRemovedTuples;

	PG_RETURN_POINTER(stats);
}

static void
PGrnRemoveUnusedTables(void)
{
	grn_table_cursor *cursor;
	const char *min = PGrnSourcesTableNamePrefix;

	cursor = grn_table_cursor_open(ctx, grn_ctx_db(ctx),
								   min, strlen(min),
								   NULL, 0,
								   0, -1, GRN_CURSOR_BY_KEY|GRN_CURSOR_PREFIX);
	while (grn_table_cursor_next(ctx, cursor) != GRN_ID_NIL) {
		char *name;
		char *nameEnd;
		int nameSize;
		Oid relationID;
		Relation relation;
		unsigned int i;

		nameSize = grn_table_cursor_get_key(ctx, cursor, (void **)&name);
		nameEnd = name + nameSize;
		relationID = strtol(name + strlen(min), &nameEnd, 10);
		if (nameEnd[0] == '.')
			continue;
		relation = RelationIdGetRelation(relationID);
		if (relation)
		{
			RelationClose(relation);
			continue;
		}

		for (i = 0; true; i++)
		{
			char tableName[GRN_TABLE_MAX_KEY_SIZE];
			grn_obj *table;

			snprintf(tableName, sizeof(tableName),
					 PGrnLexiconNameFormat, relationID, i);
			table = grn_ctx_get(ctx, tableName, strlen(tableName));
			if (!table)
				break;
			grn_obj_remove(ctx, table);
		}

		{
			char tableName[GRN_TABLE_MAX_KEY_SIZE];
			grn_obj *table;

			snprintf(tableName, sizeof(tableName),
					 PGrnSourcesTableNameFormat, relationID);
			table = grn_ctx_get(ctx, tableName, strlen(tableName));
			if (table)
			{
				grn_obj_remove(ctx, table);
			}
		}
	}
	grn_table_cursor_close(ctx, cursor);
}


/**
 * pgroonga.vacuumcleanup() -- amvacuumcleanup
 */
Datum
pgroonga_vacuumcleanup(PG_FUNCTION_ARGS)
{
	IndexVacuumInfo *info = (IndexVacuumInfo *) PG_GETARG_POINTER(0);
	IndexBulkDeleteResult *stats = (IndexBulkDeleteResult *) PG_GETARG_POINTER(1);

	if (!stats)
	{
		grn_obj *sourcesTable;
		sourcesTable = PGrnLookupSourcesTable(info->index, WARNING);
		stats = PGrnBulkDeleteResult(info, sourcesTable);
	}

	PGrnRemoveUnusedTables();

	PG_RETURN_POINTER(stats);
}

/**
 * pgroonga.costestimate() -- amcostestimate
 */
Datum
pgroonga_costestimate(PG_FUNCTION_ARGS)
{
	return gistcostestimate(fcinfo);

#ifdef NOT_USED
	PlannerInfo *root = (PlannerInfo *) PG_GETARG_POINTER(0);
	IndexPath *path = (IndexPath *) PG_GETARG_POINTER(1);
	double loopCount = PG_GETARG_FLOAT8(2);
	Cost *indexStartupCost = (Cost *) PG_GETARG_POINTER(3);
	Cost *indexTotalCost = (Cost *) PG_GETARG_POINTER(4);
	Selectivity *indexSelectivity = (Selectivity *) PG_GETARG_POINTER(5);
	double *indexCorrelation = (double *) PG_GETARG_POINTER(6);
	IndexOptInfo *index = path->indexinfo;

	/* TODO: Use more clever logic.
	 *
	 * We want to use index scan rather than bitmap scan for full text search.
	 * Because bitmap scan requires bitmap heap scan that is slow for
	 * large result set.
	 *
	 * We want to use bitmap scan rather than index scan for OR search.
	 * Because we can't use index scan for OR search.
	 *
	 * We want to use the default scan for other cases.
	 */
	*indexSelectivity = clauselist_selectivity(root,
											   path->indexquals,
											   index->rel->relid,
											   JOIN_INNER,
											   NULL);

	*indexStartupCost = 0.0;
	*indexTotalCost = 0.0;
	*indexCorrelation = 0.0;

	PG_RETURN_VOID();
#endif
}

/**
 * pgroonga.options() -- amoptions
 */
Datum
pgroonga_options(PG_FUNCTION_ARGS)
{
	Datum reloptions = PG_GETARG_DATUM(0);
	bool validate = PG_GETARG_BOOL(1);
	relopt_value *options;
	PGrnOptions *grnOptions;
	int nOptions;
	const relopt_parse_elt optionsMap[] = {
		{"tokenizer", RELOPT_TYPE_STRING,
		 offsetof(PGrnOptions, tokenizerOffset)},
		{"normalizer", RELOPT_TYPE_STRING,
		 offsetof(PGrnOptions, normalizerOffset)}
	};

	options = parseRelOptions(reloptions, validate, PGrnReloptionKind,
							  &nOptions);
	grnOptions = allocateReloptStruct(sizeof(PGrnOptions), options, nOptions);
	fillRelOptions(grnOptions, sizeof(PGrnOptions), options, nOptions,
				   validate, optionsMap, lengthof(optionsMap));
	pfree(options);

	PG_RETURN_BYTEA_P(grnOptions);
}
