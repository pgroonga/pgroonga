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
#ifdef JSONBOID
#	include <utils/jsonb.h>
#endif

#include <groonga.h>

#include <xxhash.h>

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
	grn_obj *jsonPathsTable;
	grn_obj *jsonValuesTable;
	grn_obj *supplementaryTables;
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
static grn_obj pathBuffer;
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
	GRN_OBJ_FIN(ctx, &pathBuffer);
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
	GRN_TEXT_INIT(&pathBuffer, 0);
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

#ifdef JSONBOID
static void
PGrnJSONGenerateCompletePath(grn_obj *components, grn_obj *path)
{
	unsigned int i, n;

	n = grn_vector_size(ctx, components);

	GRN_TEXT_PUTS(ctx, path, ".");
	for (i = 0; i < n; i++)
	{
		const char *component;
		unsigned int componentSize;
		grn_id domain;

		componentSize = grn_vector_get_element(ctx,
											   components,
											   i,
											   &component,
											   NULL,
											   &domain);
		if (domain == GRN_DB_UINT32)
		{
			GRN_TEXT_PUTS(ctx, path, "[]");
		}
		else
		{
			GRN_TEXT_PUTS(ctx, path, "[");
			grn_text_esc(ctx, path, component, componentSize);
			GRN_TEXT_PUTS(ctx, path, "]");
		}
	}
}

static const char *
PGrnJSONIteratorTokenName(JsonbIteratorToken token)
{
	static char *names[] = {
		"WJB_DONE",
		"WJB_KEY",
		"WJB_VALUE",
		"WJB_ELEM",
		"WJB_BEGIN_ARRAY",
		"WJB_END_ARRAY",
		"WJB_BEGIN_OBJECT",
		"WJB_END_OBJECT"
	};

	return names[token];
}
#endif

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
PGrnLookupColumn(grn_obj *table, const char *name, int errorLevel)
{
	grn_obj *column;

	column = grn_obj_column(ctx, table, name, strlen(name));
	if (!column)
	{
		char tableName[GRN_TABLE_MAX_KEY_SIZE];
		int tableNameSize;

		tableNameSize = grn_obj_name(ctx, table, tableName, sizeof(tableName));
		ereport(errorLevel,
				(errcode(ERRCODE_INVALID_NAME),
				 errmsg("pgroonga: column isn't found: <%.*s>:<%s>",
						tableNameSize, tableName,
						name)));
	}

	return column;
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

#ifdef JSONBOID
static grn_obj *
PGrnLookupJSONPathsTable(Relation index,
						 unsigned int nthAttribute,
						 int errorLevel)
{
	char name[GRN_TABLE_MAX_KEY_SIZE];

	snprintf(name, sizeof(name),
			 PGrnJSONPathsTableNameFormat,
			 index->rd_node.relNode, nthAttribute);
	return PGrnLookup(name, errorLevel);
}

static grn_obj *
PGrnLookupJSONValuesTable(Relation index,
						  unsigned int nthAttribute,
						  int errorLevel)
{
	char name[GRN_TABLE_MAX_KEY_SIZE];

	snprintf(name, sizeof(name),
			 PGrnJSONValuesTableNameFormat,
			 index->rd_node.relNode, nthAttribute);
	return PGrnLookup(name, errorLevel);
}
#endif

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

#ifdef JSONBOID
static void
PGrnCreateDataColumnsForJSON(PGrnCreateData *data)
{
	grn_obj *jsonTypesTable;

	{
		char jsonPathsTableName[GRN_TABLE_MAX_KEY_SIZE];
		snprintf(jsonPathsTableName, sizeof(jsonPathsTableName),
				 PGrnJSONPathsTableNameFormat,
				 data->relNode, data->i);
		data->jsonPathsTable =
			PGrnCreateTable(jsonPathsTableName,
							GRN_OBJ_TABLE_PAT_KEY,
							grn_ctx_at(ctx, GRN_DB_SHORT_TEXT));
		GRN_PTR_PUT(ctx, data->supplementaryTables, data->jsonPathsTable);
	}

	{
		char jsonTypesTableName[GRN_TABLE_MAX_KEY_SIZE];
		snprintf(jsonTypesTableName, sizeof(jsonTypesTableName),
				 PGrnJSONTypesTableNameFormat,
				 data->relNode, data->i);
		jsonTypesTable = PGrnCreateTable(jsonTypesTableName,
										 GRN_OBJ_TABLE_PAT_KEY,
										 grn_ctx_at(ctx, GRN_DB_SHORT_TEXT));
		GRN_PTR_PUT(ctx, data->supplementaryTables, jsonTypesTable);
	}
	{
		char jsonValuesTableName[GRN_TABLE_MAX_KEY_SIZE];
		snprintf(jsonValuesTableName, sizeof(jsonValuesTableName),
				 PGrnJSONValuesTableNameFormat,
				 data->relNode, data->i);
		data->jsonValuesTable =
			PGrnCreateTable(jsonValuesTableName,
							GRN_OBJ_TABLE_HASH_KEY,
							grn_ctx_at(ctx, GRN_DB_UINT64));
		GRN_PTR_PUT(ctx, data->supplementaryTables, data->jsonValuesTable);
	}

	PGrnCreateColumn(data->jsonValuesTable,
					 "path",
					 GRN_OBJ_COLUMN_SCALAR,
					 data->jsonPathsTable);
	PGrnCreateColumn(data->jsonValuesTable,
					 "paths",
					 GRN_OBJ_COLUMN_VECTOR,
					 data->jsonPathsTable);
	{
		grn_obj_flags flags = 0;
		if (PGrnIsLZ4Available)
			flags |= GRN_OBJ_COMPRESS_LZ4;
		PGrnCreateColumn(data->jsonValuesTable,
						 "string",
						 flags,
						 grn_ctx_at(ctx, GRN_DB_LONG_TEXT));
	}
	PGrnCreateColumn(data->jsonValuesTable,
					 "number",
					 0,
					 grn_ctx_at(ctx, GRN_DB_FLOAT));
	PGrnCreateColumn(data->jsonValuesTable,
					 "boolean",
					 0,
					 grn_ctx_at(ctx, GRN_DB_BOOL));
	PGrnCreateColumn(data->jsonValuesTable,
					 "size",
					 0,
					 grn_ctx_at(ctx, GRN_DB_UINT32));
	PGrnCreateColumn(data->jsonValuesTable,
					 "type",
					 0,
					 jsonTypesTable);
}

static void
PGrnCreateFullTextSearchIndexColumnForJSON(PGrnCreateData *data)
{
	PGrnOptions *options;
	const char *tokenizerName = PGRN_DEFAULT_TOKENIZER;
	const char *normalizerName = PGRN_DEFAULT_NORMALIZER;
	char lexiconName[GRN_TABLE_MAX_KEY_SIZE];
	grn_obj *lexicon;

	options = (PGrnOptions *) (data->index->rd_options);
	if (options)
	{
		tokenizerName = (const char *) (options) + options->tokenizerOffset;
		normalizerName = (const char *) (options) + options->normalizerOffset;
	}

	if (!tokenizerName || tokenizerName[0] == '\0')
		return;

	snprintf(lexiconName, sizeof(lexiconName),
			 PGrnJSONValueLexiconNameFormat,
			 "FullTextSearch", data->relNode, data->i);
	lexicon = PGrnCreateTable(lexiconName,
							  GRN_OBJ_TABLE_PAT_KEY,
							  grn_ctx_at(ctx, GRN_DB_SHORT_TEXT));
	GRN_PTR_PUT(ctx, data->lexicons, lexicon);

	grn_obj_set_info(ctx, lexicon, GRN_INFO_DEFAULT_TOKENIZER,
					 PGrnLookup(tokenizerName, ERROR));
	if (normalizerName && normalizerName[0])
	{
		grn_obj_set_info(ctx, lexicon, GRN_INFO_NORMALIZER,
						 PGrnLookup(normalizerName, ERROR));
	}

	PGrnCreateColumn(lexicon,
					 PGrnIndexColumnName,
					 GRN_OBJ_COLUMN_INDEX | GRN_OBJ_WITH_POSITION,
					 data->jsonValuesTable);
}

static void
PGrnCreateIndexColumnForJSON(PGrnCreateData *data,
							 const char *typeName,
							 grn_obj_flags tableType,
							 grn_obj *type)
{
	char lexiconName[GRN_TABLE_MAX_KEY_SIZE];
	grn_obj *lexicon;

	snprintf(lexiconName, sizeof(lexiconName),
			 PGrnJSONValueLexiconNameFormat,
			 typeName, data->relNode, data->i);
	lexicon = PGrnCreateTable(lexiconName, tableType, type);
	GRN_PTR_PUT(ctx, data->lexicons, lexicon);
	PGrnCreateColumn(lexicon,
					 PGrnIndexColumnName,
					 GRN_OBJ_COLUMN_INDEX,
					 data->jsonValuesTable);
}

static void
PGrnCreateIndexColumnsForJSON(PGrnCreateData *data)
{
	PGrnCreateColumn(data->jsonValuesTable,
					 PGrnIndexColumnName,
					 GRN_OBJ_COLUMN_INDEX,
					 data->sourcesTable);
	PGrnCreateColumn(data->jsonPathsTable,
					 PGrnIndexColumnName,
					 GRN_OBJ_COLUMN_INDEX | GRN_OBJ_WITH_SECTION,
					 data->jsonValuesTable);

	/* TODO: 4KiB over string value can't be searched. */
	PGrnCreateIndexColumnForJSON(data,
								 "String",
								 GRN_OBJ_TABLE_PAT_KEY,
								 grn_ctx_at(ctx, GRN_DB_SHORT_TEXT));
	PGrnCreateIndexColumnForJSON(data,
								 "Number",
								 GRN_OBJ_TABLE_PAT_KEY,
								 grn_ctx_at(ctx, GRN_DB_FLOAT));
	PGrnCreateIndexColumnForJSON(data,
								 "Boolean",
								 GRN_OBJ_TABLE_HASH_KEY,
								 grn_ctx_at(ctx, GRN_DB_BOOL));
	PGrnCreateIndexColumnForJSON(data,
								 "Size",
								 GRN_OBJ_TABLE_PAT_KEY,
								 grn_ctx_at(ctx, GRN_DB_UINT32));

	PGrnCreateFullTextSearchIndexColumnForJSON(data);
}
#endif

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
 */
static void
PGrnCreate(Relation index,
		   grn_obj **sourcesTable,
		   grn_obj **sourcesCtidColumn,
		   grn_obj *supplementaryTables,
		   grn_obj *lexicons)
{
	PGrnCreateData data;

	data.index = index;
	data.desc = RelationGetDescr(index);
	data.relNode = index->rd_node.relNode;
	data.jsonPathsTable = NULL;
	data.jsonValuesTable = NULL;
	data.supplementaryTables = supplementaryTables;
	data.lexicons = lexicons;

	PGrnCreateSourcesTable(&data);
	*sourcesTable = data.sourcesTable;
	*sourcesCtidColumn = data.sourcesCtidColumn;

	for (data.i = 0; data.i < data.desc->natts; data.i++)
	{
#ifdef JSONBOID
		Form_pg_attribute attribute;

		attribute = data.desc->attrs[data.i];
		if (attribute->atttypid == JSONBOID)
		{
			if (data.desc->natts != 1)
			{
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						 errmsg("pgroonga: multicolumn index for jsonb "
								"isn't supported: <%s>",
								index->rd_rel->relname.data)));
			}

			PGrnCreateDataColumnsForJSON(&data);
			PGrnCreateIndexColumnsForJSON(&data);
			data.forFullTextSearch = false;
			data.attributeTypeID = grn_obj_id(ctx, data.jsonValuesTable);
			data.attributeFlags = GRN_OBJ_VECTOR;
			PGrnCreateDataColumn(&data);
		}
		else
#endif
		{
			data.forFullTextSearch = PGrnIsForFullTextSearchIndex(index, data.i);
			data.attributeTypeID = PGrnGetType(index, data.i,
											   &(data.attributeFlags));
			PGrnCreateDataColumn(&data);
			PGrnCreateIndexColumn(&data);
		}
	}
}

static void
PGrnSetSource(grn_obj *indexColumn,
			  grn_obj *source,
			  grn_obj *sourceIDs)
{
	grn_id sourceID;

	GRN_BULK_REWIND(sourceIDs);

	sourceID = grn_obj_id(ctx, source);
	GRN_RECORD_PUT(ctx, sourceIDs, sourceID);

	grn_obj_set_info(ctx, indexColumn, GRN_INFO_SOURCE, sourceIDs);
}

#ifdef JSONBOID
static void
PGrnSetSourceForJSON(Relation index,
					 grn_obj *jsonValuesTable,
					 const char *columnName,
					 const char *typeName,
					 unsigned int nthAttribute,
					 grn_obj *sourceIDs,
					 bool required)
{
	char indexName[GRN_TABLE_MAX_KEY_SIZE];
	grn_obj *indexColumn;
	grn_obj *source;

	snprintf(indexName, sizeof(indexName),
			 PGrnJSONValueLexiconNameFormat ".%s",
			 typeName,
			 index->rd_node.relNode,
			 nthAttribute,
			 PGrnIndexColumnName);
	if (required)
	{
		indexColumn = PGrnLookup(indexName, ERROR);
	}
	else
	{
		indexColumn = grn_ctx_get(ctx, indexName, strlen(indexName));
		if (!indexColumn)
			return;
	}

	source = PGrnLookupColumn(jsonValuesTable, columnName, ERROR);
	PGrnSetSource(indexColumn, source, sourceIDs);

	grn_obj_unlink(ctx, source);
	grn_obj_unlink(ctx, indexColumn);
}

static void
PGrnSetSourcesForJSON(Relation index,
					  grn_obj *jsonValuesTable,
					  unsigned int nthAttribute,
					  grn_obj *sourceIDs)
{
	grn_obj *jsonPathsTable;

	jsonPathsTable = PGrnLookupJSONPathsTable(index, nthAttribute, ERROR);

	{
		grn_obj *source;
		grn_obj *indexColumn;

		GRN_BULK_REWIND(sourceIDs);

		source = PGrnLookupColumn(jsonValuesTable, "path", ERROR);
		GRN_RECORD_PUT(ctx, sourceIDs, grn_obj_id(ctx, source));
		grn_obj_unlink(ctx, source);

		source = PGrnLookupColumn(jsonValuesTable, "paths", ERROR);
		GRN_RECORD_PUT(ctx, sourceIDs, grn_obj_id(ctx, source));
		grn_obj_unlink(ctx, source);

		indexColumn = PGrnLookupColumn(jsonPathsTable, PGrnIndexColumnName,
									   ERROR);
		grn_obj_set_info(ctx, indexColumn, GRN_INFO_SOURCE, sourceIDs);
		grn_obj_unlink(ctx, indexColumn);
	}

	PGrnSetSourceForJSON(index, jsonValuesTable, "string", "String",
						 nthAttribute, sourceIDs, true);
	PGrnSetSourceForJSON(index, jsonValuesTable, "number", "Number",
						 nthAttribute, sourceIDs, true);
	PGrnSetSourceForJSON(index, jsonValuesTable, "boolean", "Boolean",
						 nthAttribute, sourceIDs, true);
	PGrnSetSourceForJSON(index, jsonValuesTable, "size", "Size",
						 nthAttribute, sourceIDs, true);
	PGrnSetSourceForJSON(index, jsonValuesTable, "string", "FullTextSearch",
						 nthAttribute, sourceIDs, false);

	grn_obj_unlink(ctx, jsonPathsTable);
}
#endif

static void
PGrnSetSources(Relation index, grn_obj *sourcesTable)
{
	TupleDesc desc;
	grn_obj sourceIDs;
	unsigned int i;

	desc = RelationGetDescr(index);
	GRN_RECORD_INIT(&sourceIDs, GRN_OBJ_VECTOR, GRN_ID_NIL);
	for (i = 0; i < desc->natts; i++)
	{
		Form_pg_attribute attribute = desc->attrs[i];
		NameData *name = &(attribute->attname);
		grn_obj *source;
		grn_obj *indexColumn;

#ifdef JSONBOID
		if (attribute->atttypid == JSONBOID)
		{
			grn_obj *jsonValuesTable;

			jsonValuesTable = PGrnLookupJSONValuesTable(index, i, ERROR);
			PGrnSetSourcesForJSON(index, jsonValuesTable, i, &sourceIDs);
			indexColumn = PGrnLookupColumn(jsonValuesTable,
										   PGrnIndexColumnName,
										   ERROR);
			grn_obj_unlink(ctx, jsonValuesTable);
		}
		else
#endif
		{
			indexColumn = PGrnLookupIndexColumn(index, i, ERROR);
		}

		source = PGrnLookupColumn(sourcesTable, name->data, ERROR);
		PGrnSetSource(indexColumn, source, &sourceIDs);
		grn_obj_unlink(ctx, source);
		grn_obj_unlink(ctx, indexColumn);
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

/**
 * pgroonga.match_jsonb(jsonb, query) : bool
 */
Datum
pgroonga_match_jsonb(PG_FUNCTION_ARGS)
{
#ifdef NOT_USED
	Jsonb *jsonb = PG_GETARG_JSONB(0);
	text *query = PG_GETARG_TEXT_PP(1);
#endif

	ereport(ERROR,
			(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
			 errmsg("pgroonga: operator @@ is available only in index scans")));

	PG_RETURN_BOOL(false);
}

#ifdef JSONBOID
typedef struct PGrnInsertJSONData
{
	grn_obj *jsonPathsTable;
	grn_obj *jsonValuesTable;
	grn_obj *pathColumn;
	grn_obj *pathsColumn;
	grn_obj *stringColumn;
	grn_obj *numberColumn;
	grn_obj *booleanColumn;
	grn_obj *sizeColumn;
	grn_obj *typeColumn;
	grn_obj *valueIDs;
	grn_obj key;
	grn_obj components;
	grn_obj path;
	grn_obj pathIDs;
	grn_obj value;
	grn_obj type;
} PGrnInsertJSONData;

static void
PGrnInsertJSONDataInit(PGrnInsertJSONData *data,
					   Relation index,
					   unsigned int nthValue,
					   grn_obj *valueIDs)
{
	data->jsonPathsTable  = PGrnLookupJSONPathsTable(index, nthValue, ERROR);
	data->jsonValuesTable = PGrnLookupJSONValuesTable(index, nthValue, ERROR);

	data->pathColumn =
		PGrnLookupColumn(data->jsonValuesTable, "path", ERROR);
	data->pathsColumn =
		PGrnLookupColumn(data->jsonValuesTable, "paths", ERROR);
	data->stringColumn =
		PGrnLookupColumn(data->jsonValuesTable, "string", ERROR);
	data->numberColumn =
		PGrnLookupColumn(data->jsonValuesTable, "number", ERROR);
	data->booleanColumn =
		PGrnLookupColumn(data->jsonValuesTable, "boolean", ERROR);
	data->sizeColumn =
		PGrnLookupColumn(data->jsonValuesTable, "size", ERROR);
	data->typeColumn =
		PGrnLookupColumn(data->jsonValuesTable, "type", ERROR);

	data->valueIDs = valueIDs;
	grn_obj_reinit(ctx, data->valueIDs,
				   grn_obj_id(ctx, data->jsonValuesTable),
				   GRN_OBJ_VECTOR);

	GRN_TEXT_INIT(&(data->key), 0);
	GRN_TEXT_INIT(&(data->components), GRN_OBJ_VECTOR);
	GRN_TEXT_INIT(&(data->path), 0);
	GRN_RECORD_INIT(&(data->pathIDs), GRN_OBJ_VECTOR,
					grn_obj_id(ctx, data->jsonPathsTable));
	GRN_VOID_INIT(&(data->value));
	GRN_TEXT_INIT(&(data->type), GRN_OBJ_DO_SHALLOW_COPY);
}

static void
PGrnInsertJSONDataFin(PGrnInsertJSONData *data)
{
	GRN_OBJ_FIN(ctx, &(data->type));
	GRN_OBJ_FIN(ctx, &(data->value));
	GRN_OBJ_FIN(ctx, &(data->pathIDs));
	GRN_OBJ_FIN(ctx, &(data->path));
	GRN_OBJ_FIN(ctx, &(data->components));
	GRN_OBJ_FIN(ctx, &(data->key));

	grn_obj_unlink(ctx, data->typeColumn);
	grn_obj_unlink(ctx, data->sizeColumn);
	grn_obj_unlink(ctx, data->booleanColumn);
	grn_obj_unlink(ctx, data->numberColumn);
	grn_obj_unlink(ctx, data->stringColumn);
	grn_obj_unlink(ctx, data->pathsColumn);
	grn_obj_unlink(ctx, data->pathColumn);
	grn_obj_unlink(ctx, data->jsonValuesTable);
	grn_obj_unlink(ctx, data->jsonPathsTable);
}

static uint64_t
PGrnInsertJSONGenerateKey(PGrnInsertJSONData *data,
						  bool haveValue,
						  const char *typeName)
{
	unsigned int i, n;

	GRN_BULK_REWIND(&(data->key));

	GRN_TEXT_PUTS(ctx, &(data->key), ".");
	n = grn_vector_size(ctx, &(data->components));
	for (i = 0; i < n; i++)
	{
		const char *component;
		unsigned int componentSize;
		grn_id domain;

		componentSize = grn_vector_get_element(ctx,
											   &(data->components),
											   i,
											   &component,
											   NULL,
											   &domain);
		if (domain == GRN_DB_UINT32)
		{
			GRN_TEXT_PUTS(ctx, &(data->key), "[]");
		}
		else
		{
			GRN_TEXT_PUTS(ctx, &(data->key), "[");
			grn_text_esc(ctx, &(data->key), component, componentSize);
			GRN_TEXT_PUTS(ctx, &(data->key), "]");
		}
	}

	GRN_TEXT_PUTS(ctx, &(data->key), "|");
	GRN_TEXT_PUTS(ctx, &(data->key), typeName);

	if (haveValue)
	{
		GRN_TEXT_PUTS(ctx, &(data->key), "|");
		grn_obj_cast(ctx, &(data->value), &(data->key), GRN_FALSE);
	}

	return XXH64(GRN_TEXT_VALUE(&data->key),
				 GRN_TEXT_LEN(&data->key),
				 0);
}

static void
PGrnInsertJSONAddPath(PGrnInsertJSONData *data)
{
	grn_id pathID;

	if (GRN_TEXT_LEN(&(data->path)) >= GRN_TABLE_MAX_KEY_SIZE)
		return;

	pathID = grn_table_add(ctx, data->jsonPathsTable,
						   GRN_TEXT_VALUE(&(data->path)),
						   GRN_TEXT_LEN(&(data->path)),
						   NULL);
	if (pathID == GRN_ID_NIL)
		return;

	GRN_RECORD_PUT(ctx, &(data->pathIDs), pathID);
}

static void
PGrnInsertJSONGenerateSubPathsRecursive(PGrnInsertJSONData *data,
										unsigned int parentStart,
										unsigned int current)
{
	unsigned int i;

	GRN_BULK_REWIND(&(data->path));
	for (i = parentStart; i <= current; i++)
	{
		const char *component;
		unsigned int componentSize;
		grn_id domain;

		componentSize = grn_vector_get_element(ctx,
											   &(data->components),
											   i,
											   &component,
											   NULL,
											   &domain);
		if (domain == GRN_DB_UINT32)
			continue;
		if (GRN_TEXT_LEN(&(data->path)) > 0)
			GRN_TEXT_PUTS(ctx, &(data->path), ".");
		GRN_TEXT_PUT(ctx, &(data->path), component, componentSize);
	}
	PGrnInsertJSONAddPath(data);

	GRN_BULK_REWIND(&(data->path));
	for (i = parentStart; i <= current; i++)
	{
		const char *component;
		unsigned int componentSize;
		grn_id domain;

		componentSize = grn_vector_get_element(ctx,
											   &(data->components),
											   i,
											   &component,
											   NULL,
											   &domain);
		if (domain == GRN_DB_UINT32)
			continue;
		GRN_TEXT_PUTS(ctx, &(data->path), "[");
		grn_text_esc(ctx, &(data->path), component, componentSize);
		GRN_TEXT_PUTS(ctx, &(data->path), "]");
	}
	PGrnInsertJSONAddPath(data);

	if (parentStart < current)
		PGrnInsertJSONGenerateSubPathsRecursive(data, parentStart + 1, current);
}

static void
PGrnInsertJSONGeneratePathsRecursive(PGrnInsertJSONData *data,
									 unsigned int current,
									 unsigned int nComponents)
{
	unsigned int i;

	if (current == nComponents)
		return;

	GRN_BULK_REWIND(&(data->path));
	GRN_TEXT_PUTS(ctx, &(data->path), ".");
	for (i = 0; i <= current; i++)
	{
		const char *component;
		unsigned int componentSize;
		grn_id domain;

		componentSize = grn_vector_get_element(ctx,
											   &(data->components),
											   i,
											   &component,
											   NULL,
											   &domain);
		if (domain == GRN_DB_UINT32)
			continue;
		if (GRN_TEXT_LEN(&(data->path)) > 1)
			GRN_TEXT_PUTS(ctx, &(data->path), ".");
		GRN_TEXT_PUT(ctx, &(data->path), component, componentSize);
	}
	PGrnInsertJSONAddPath(data);

	GRN_BULK_REWIND(&(data->path));
	GRN_TEXT_PUTS(ctx, &(data->path), ".");
	for (i = 0; i <= current; i++)
	{
		const char *component;
		unsigned int componentSize;
		grn_id domain;

		componentSize = grn_vector_get_element(ctx,
											   &(data->components),
											   i,
											   &component,
											   NULL,
											   &domain);
		if (domain == GRN_DB_UINT32)
			continue;
		GRN_TEXT_PUTS(ctx, &(data->path), "[");
		grn_text_esc(ctx, &(data->path), component, componentSize);
		GRN_TEXT_PUTS(ctx, &(data->path), "]");
	}
	PGrnInsertJSONAddPath(data);

	PGrnInsertJSONGenerateSubPathsRecursive(data, 0, current);
	PGrnInsertJSONGeneratePathsRecursive(data, current + 1, nComponents);
}

static void
PGrnInsertJSONGeneratePaths(PGrnInsertJSONData *data)
{
	unsigned int n;

	GRN_BULK_REWIND(&(data->pathIDs));
	n = grn_vector_size(ctx, &(data->components));
	PGrnInsertJSONGeneratePathsRecursive(data, 0, n);
}

static void
PGrnInsertJSONValueSet(PGrnInsertJSONData *data,
					   grn_obj *column,
					   const char *typeName)
{
	uint64_t key;
	grn_id valueID;
	int added;

	key = PGrnInsertJSONGenerateKey(data, column != NULL, typeName);
	valueID = grn_table_add(ctx, data->jsonValuesTable,
							&key, sizeof(uint64_t),
							&added);
	GRN_RECORD_PUT(ctx, data->valueIDs, valueID);
	if (!added)
		return;

	GRN_BULK_REWIND(&(data->path));
	PGrnJSONGenerateCompletePath(&(data->components), &(data->path));
	if (GRN_TEXT_LEN(&(data->path)) < GRN_TABLE_MAX_KEY_SIZE)
		grn_obj_set_value(ctx, data->pathColumn, valueID,
						  &(data->path), GRN_OBJ_SET);

	PGrnInsertJSONGeneratePaths(data);
	grn_obj_set_value(ctx, data->pathsColumn, valueID,
					  &(data->pathIDs), GRN_OBJ_SET);

	if (column)
		grn_obj_set_value(ctx, column, valueID, &(data->value), GRN_OBJ_SET);

	GRN_TEXT_SETS(ctx, &(data->type), typeName);
	grn_obj_set_value(ctx, data->typeColumn, valueID,
					  &(data->type), GRN_OBJ_SET);
}

static void PGrnInsertJSON(JsonbIterator **iter, PGrnInsertJSONData *data);

static void
PGrnInsertJSONValue(JsonbIterator **iter,
					JsonbValue *value,
					PGrnInsertJSONData *data)
{
	switch (value->type)
	{
	case jbvNull:
		PGrnInsertJSONValueSet(data, NULL, "null");
		break;
	case jbvString:
		grn_obj_reinit(ctx, &(data->value), GRN_DB_LONG_TEXT,
					   GRN_OBJ_DO_SHALLOW_COPY);
		GRN_TEXT_SET(ctx, &(data->value),
					 value->val.string.val,
					 value->val.string.len);
		PGrnInsertJSONValueSet(data, data->stringColumn, "string");
		break;
	case jbvNumeric:
	{
		Datum numericInString =
			DirectFunctionCall1(numeric_out,
								NumericGetDatum(value->val.numeric));
		const char *numericInCString = DatumGetCString(numericInString);
		grn_obj_reinit(ctx, &(data->value), GRN_DB_TEXT,
					   GRN_OBJ_DO_SHALLOW_COPY);
		GRN_TEXT_SETS(ctx, &(data->value), numericInCString);
		PGrnInsertJSONValueSet(data, data->numberColumn, "number");
		break;
	}
	case jbvBool:
		grn_obj_reinit(ctx, &(data->value), GRN_DB_BOOL, 0);
		GRN_BOOL_SET(ctx, &(data->value), value->val.boolean);
		PGrnInsertJSONValueSet(data, data->booleanColumn, "boolean");
		break;
	case jbvArray:
		PGrnInsertJSON(iter, data);
		break;
	case jbvObject:
		PGrnInsertJSON(iter, data);
		break;
	case jbvBinary:
		PGrnInsertJSON(iter, data);
		break;
	}
}

static void
PGrnInsertJSON(JsonbIterator **iter, PGrnInsertJSONData *data)
{
	JsonbIteratorToken token;
	JsonbValue value;

	while ((token = JsonbIteratorNext(iter, &value, false)) != WJB_DONE) {
		switch (token)
		{
		case WJB_KEY:
			grn_vector_add_element(ctx, &(data->components),
								   value.val.string.val,
								   value.val.string.len,
								   0,
								   GRN_DB_SHORT_TEXT);
			break;
		case WJB_VALUE:
			PGrnInsertJSONValue(iter, &value, data);
			{
				const char *component;
				grn_vector_pop_element(ctx, &(data->components), &component,
									   NULL, NULL);
			}
			break;
		case WJB_ELEM:
			PGrnInsertJSONValue(iter, &value, data);
			break;
		case WJB_BEGIN_ARRAY:
		{
			uint32_t nElements = value.val.array.nElems;
			grn_vector_add_element(ctx, &(data->components),
								   (const char *)&nElements,
								   sizeof(uint32_t),
								   0,
								   GRN_DB_UINT32);
			PGrnInsertJSONValueSet(data, NULL, "array");
			break;
		}
		case WJB_END_ARRAY:
		{
			const char *component;
			grn_vector_pop_element(ctx, &(data->components), &component,
								   NULL, NULL);
			break;
		}
		case WJB_BEGIN_OBJECT:
			PGrnInsertJSONValueSet(data, NULL, "object");
			break;
		case WJB_END_OBJECT:
			break;
		default:
			ereport(ERROR,
					(errcode(ERRCODE_SYSTEM_ERROR),
					 errmsg("pgroonga: jsonb iterator returns invalid token: %d",
							token)));
			break;
		}
	}
}

static void
PGrnInsertForJSON(Relation index,
				  Datum *values,
				  unsigned int nthValue,
				  grn_obj *valueIDs)
{
	PGrnInsertJSONData data;
	Jsonb *jsonb;
	JsonbIterator *iter;

	PGrnInsertJSONDataInit(&data, index, nthValue, valueIDs);
	jsonb = DatumGetJsonb(values[nthValue]);
	iter = JsonbIteratorInit(&(jsonb->root));
	PGrnInsertJSON(&iter, &data);
	PGrnInsertJSONDataFin(&data);
}
#endif

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
	unsigned int i;

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
#ifdef JSONBOID
		if (attribute->atttypid == JSONBOID)
		{
			PGrnInsertForJSON(index, values, i, &buffer);
		}
		else
#endif
		{
			domain = PGrnGetType(index, i, &flags);
			grn_obj_reinit(ctx, &buffer, domain, flags);
			PGrnConvertDatum(values[i], attribute->atttypid, &buffer);
		}
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

#ifdef JSONBOID
static void
PGrnSearchBuildConditionJSONQuery(PGrnSearchData *data,
								  grn_obj *subFilter,
								  grn_obj *targetColumn,
								  grn_obj *filter,
								  unsigned int *nthCondition)
{
	grn_expr_append_obj(ctx, data->expression,
						subFilter, GRN_OP_PUSH, 1);
	grn_expr_append_obj(ctx, data->expression,
						targetColumn, GRN_OP_PUSH, 1);
	grn_expr_append_const(ctx, data->expression,
						  filter, GRN_OP_PUSH, 1);
	grn_expr_append_op(ctx, data->expression, GRN_OP_CALL, 2);

	if (*nthCondition > 0)
		grn_expr_append_op(ctx, data->expression, GRN_OP_AND, 2);

	(*nthCondition)++;
}

static void
PGrnSearchBuildConditionJSONContainType(PGrnSearchData *data,
										grn_obj *subFilter,
										grn_obj *targetColumn,
										grn_obj *components,
										const char *typeName,
										unsigned int *nthCondition)
{
	GRN_BULK_REWIND(&buffer);

	GRN_TEXT_PUTS(ctx, &buffer, "type == ");
	grn_text_esc(ctx, &buffer, typeName, strlen(typeName));

	GRN_BULK_REWIND(&pathBuffer);
	PGrnJSONGenerateCompletePath(components, &pathBuffer);
	GRN_TEXT_PUTS(ctx, &buffer, " && path == ");
	grn_text_esc(ctx, &buffer,
				 GRN_TEXT_VALUE(&pathBuffer),
				 GRN_TEXT_LEN(&pathBuffer));

	PGrnSearchBuildConditionJSONQuery(data, subFilter, targetColumn,
									  &buffer, nthCondition);
}

static void
PGrnSearchBuildConditionJSONContainValue(PGrnSearchData *data,
										 grn_obj *subFilter,
										 grn_obj *targetColumn,
										 grn_obj *components,
										 JsonbValue *value,
										 unsigned int *nthCondition)
{
	GRN_BULK_REWIND(&buffer);

	switch (value->type)
	{
	case jbvNull:
		GRN_TEXT_PUTS(ctx, &buffer, "type == \"null\"");
		break;
	case jbvString:
		if (value->val.string.len == 0) {
			GRN_TEXT_PUTS(ctx, &buffer, "type == \"string\" && ");
		}
		GRN_TEXT_PUTS(ctx, &buffer, "string == ");
		grn_text_esc(ctx, &buffer,
					 value->val.string.val,
					 value->val.string.len);
		break;
	case jbvNumeric:
	{
		Datum numericInString =
			DirectFunctionCall1(numeric_out,
								NumericGetDatum(value->val.numeric));
		const char *numericInCString = DatumGetCString(numericInString);

		if (strcmp(numericInCString, "0") == 0) {
			GRN_TEXT_PUTS(ctx, &buffer, "type == \"number\" && ");
		}
		GRN_TEXT_PUTS(ctx, &buffer, "number == ");
		GRN_TEXT_PUTS(ctx, &buffer, numericInCString);
		break;
	}
	case jbvBool:
		GRN_TEXT_PUTS(ctx, &buffer, "type == \"boolean\" && ");
		GRN_TEXT_PUTS(ctx, &buffer, "boolean == ");
		if (value->val.boolean)
			GRN_TEXT_PUTS(ctx, &buffer, "true");
		else
			GRN_TEXT_PUTS(ctx, &buffer, "false");
		break;
	default:
		return;
		break;
	}

	GRN_BULK_REWIND(&pathBuffer);
	PGrnJSONGenerateCompletePath(components, &pathBuffer);
	GRN_TEXT_PUTS(ctx, &buffer, " && path == ");
	grn_text_esc(ctx, &buffer,
				 GRN_TEXT_VALUE(&pathBuffer),
				 GRN_TEXT_LEN(&pathBuffer));

	PGrnSearchBuildConditionJSONQuery(data, subFilter, targetColumn,
									  &buffer, nthCondition);
}

static void
PGrnSearchBuildConditionJSONContain(PGrnSearchData *data,
									grn_obj *subFilter,
									grn_obj *targetColumn,
									Jsonb *jsonb)
{
	unsigned int nthCondition = 0;
	grn_obj components;
	JsonbIterator *iter;
	JsonbIteratorToken token;
	JsonbValue value;

	GRN_TEXT_INIT(&components, GRN_OBJ_VECTOR);
	iter = JsonbIteratorInit(&(jsonb->root));
	while ((token = JsonbIteratorNext(&iter, &value, false)) != WJB_DONE) {
		switch (token)
		{
		case WJB_KEY:
			grn_vector_add_element(ctx, &components,
								   value.val.string.val,
								   value.val.string.len,
								   0,
								   GRN_DB_SHORT_TEXT);
			break;
		case WJB_VALUE:
		{
			const char *component;
			PGrnSearchBuildConditionJSONContainValue(data,
													 subFilter,
													 targetColumn,
													 &components,
													 &value,
													 &nthCondition);
			grn_vector_pop_element(ctx, &components, &component, NULL, NULL);
			break;
		}
		case WJB_ELEM:
			PGrnSearchBuildConditionJSONContainValue(data,
													 subFilter,
													 targetColumn,
													 &components,
													 &value,
													 &nthCondition);
			break;
		case WJB_BEGIN_ARRAY:
		{
			uint32_t nElements = value.val.array.nElems;
			grn_vector_add_element(ctx, &components,
								   (const char *)&nElements,
								   sizeof(uint32_t),
								   0,
								   GRN_DB_UINT32);
			if (nElements == 0)
				PGrnSearchBuildConditionJSONContainType(data,
														subFilter,
														targetColumn,
														&components,
														"array",
														&nthCondition);
			break;
		}
		case WJB_END_ARRAY:
		{
			const char *component;
			grn_vector_pop_element(ctx, &components, &component,
								   NULL, NULL);
			break;
		}
		case WJB_BEGIN_OBJECT:
			if (value.val.object.nPairs == 0)
				PGrnSearchBuildConditionJSONContainType(data,
														subFilter,
														targetColumn,
														&components,
														"object",
														&nthCondition);
			break;
		case WJB_END_OBJECT:
			break;
		default:
			ereport(ERROR,
					(errcode(ERRCODE_SYSTEM_ERROR),
					 errmsg("pgroonga: jsonb iterator returns invalid token: %d",
							token)));
			break;
		}
	}
	GRN_OBJ_FIN(ctx, &components);
}

static bool
PGrnSearchBuildConditionJSON(PGrnSearchData *data,
							 ScanKey key,
							 grn_obj *targetColumn)
{
	grn_obj *subFilter;

	subFilter = PGrnLookup("sub_filter", ERROR);
	grn_obj_reinit(ctx, &buffer, GRN_DB_TEXT, 0);

	if (key->sk_strategy == PGrnQueryStrategyNumber)
	{
		unsigned int nthCondition = 0;
		PGrnConvertDatum(key->sk_argument, TEXTOID, &buffer);
		PGrnSearchBuildConditionJSONQuery(data,
										  subFilter,
										  targetColumn,
										  &buffer,
										  &nthCondition);
	}
	else
	{
		PGrnSearchBuildConditionJSONContain(data,
											subFilter,
											targetColumn,
											DatumGetJsonb(key->sk_argument));
	}

	return true;
}
#endif

static bool
PGrnSearchBuildCondition(IndexScanDesc scan,
						 PGrnScanOpaque so,
						 PGrnSearchData *data,
						 int i)
{
	Relation index = scan->indexRelation;
	ScanKey key = &(scan->keyData[i]);
	TupleDesc desc;
	Form_pg_attribute attribute;
	const char *targetColumnName;
	grn_obj *targetColumn;
	grn_obj *matchTarget, *matchTargetVariable;
	grn_operator operator = GRN_OP_NOP;

	/* NULL key is not supported */
	if (key->sk_flags & SK_ISNULL)
		return false;

	desc = RelationGetDescr(index);
	attribute = desc->attrs[key->sk_attno - 1];

	targetColumnName = attribute->attname.data;
	targetColumn = PGrnLookupColumn(so->sourcesTable, targetColumnName, ERROR);
	GRN_PTR_PUT(ctx, &(data->targetColumns), targetColumn);

#ifdef JSONBOID
	if (attribute->atttypid == JSONBOID)
		return PGrnSearchBuildConditionJSON(data, key, targetColumn);
#endif

	GRN_EXPR_CREATE_FOR_QUERY(ctx, so->sourcesTable,
							  matchTarget, matchTargetVariable);
	GRN_PTR_PUT(ctx, &(data->matchTargets), matchTarget);

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
		break;
	}

	switch (key->sk_strategy)
	{
	case PGrnLikeStrategyNumber:
		PGrnSearchBuildConditionLike(data, matchTarget, &buffer);
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

	return true;
}

static void
PGrnSearchBuildConditions(IndexScanDesc scan,
						  PGrnScanOpaque so,
						  PGrnSearchData *data)
{
	int i, nExpressions = 0;

	for (i = 0; i < scan->numberOfKeys; i++)
	{
		if (!PGrnSearchBuildCondition(scan, so, data, i))
			continue;

		if (data->isEmptyCondition)
			return;

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
	grn_obj supplementaryTables;
	grn_obj lexicons;

	if (indexInfo->ii_Unique)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("pgroonga: unique index isn't supported")));

	bs.sourcesTable = NULL;
	bs.nIndexedTuples = 0.0;

	GRN_PTR_INIT(&supplementaryTables, GRN_OBJ_VECTOR, GRN_ID_NIL);
	GRN_PTR_INIT(&lexicons, GRN_OBJ_VECTOR, GRN_ID_NIL);
	PG_TRY();
	{
		PGrnCreate(index,
				   &(bs.sourcesTable),
				   &(bs.sourcesCtidColumn),
				   &supplementaryTables,
				   &lexicons);
		nHeapTuples = IndexBuildHeapScan(heap, index, indexInfo, true,
										 PGrnBuildCallback, &bs);
		PGrnSetSources(index, bs.sourcesTable);
	}
	PG_CATCH();
	{
		size_t i, n;

		n = GRN_BULK_VSIZE(&lexicons) / sizeof(grn_obj *);
		for (i = 0; i < n; i++)
		{
			grn_obj *lexicon;
			lexicon = GRN_PTR_VALUE_AT(&lexicons, i);
			grn_obj_remove(ctx, lexicon);
		}
		GRN_OBJ_FIN(ctx, &lexicons);

		n = GRN_BULK_VSIZE(&supplementaryTables) / sizeof(grn_obj *);
		for (i = 0; i < n; i++)
		{
			grn_obj *supplementaryTable;
			supplementaryTable = GRN_PTR_VALUE_AT(&supplementaryTables, i);
			grn_obj_remove(ctx, supplementaryTable);
		}
		GRN_OBJ_FIN(ctx, &supplementaryTables);

		if (bs.sourcesTable)
			grn_obj_remove(ctx, bs.sourcesTable);

		PG_RE_THROW();
	}
	PG_END_TRY();
	GRN_OBJ_FIN(ctx, &lexicons);
	GRN_OBJ_FIN(ctx, &supplementaryTables);

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
	grn_obj supplementaryTables;
	grn_obj lexicons;

	GRN_PTR_INIT(&supplementaryTables, GRN_OBJ_VECTOR, GRN_ID_NIL);
	GRN_PTR_INIT(&lexicons, GRN_OBJ_VECTOR, GRN_ID_NIL);
	PG_TRY();
	{
		PGrnCreate(index,
				   &sourcesTable,
				   &sourcesCtidColumn,
				   &supplementaryTables,
				   &lexicons);
		PGrnSetSources(index, sourcesTable);
	}
	PG_CATCH();
	{
		size_t i, n;

		n = GRN_BULK_VSIZE(&lexicons) / sizeof(grn_obj *);
		for (i = 0; i < n; i++)
		{
			grn_obj *lexicon;
			lexicon = GRN_PTR_VALUE_AT(&lexicons, i);
			grn_obj_remove(ctx, lexicon);
		}
		GRN_OBJ_FIN(ctx, &lexicons);

		n = GRN_BULK_VSIZE(&supplementaryTables) / sizeof(grn_obj *);
		for (i = 0; i < n; i++)
		{
			grn_obj *supplementaryTable;
			supplementaryTable = GRN_PTR_VALUE_AT(&supplementaryTables, i);
			grn_obj_remove(ctx, supplementaryTable);
		}
		GRN_OBJ_FIN(ctx, &supplementaryTables);

		if (sourcesTable)
			grn_obj_remove(ctx, sourcesTable);

		PG_RE_THROW();
	}
	PG_END_TRY();
	GRN_OBJ_FIN(ctx, &lexicons);
	GRN_OBJ_FIN(ctx, &supplementaryTables);

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

static void
PGrnJSONValuesUpdateDeletedID(grn_obj *jsonValuesTable,
							  grn_obj *values,
							  grn_obj *valueMin,
							  grn_obj *valueMax)
{
	unsigned int i, n;

	n = GRN_BULK_VSIZE(values) / sizeof(grn_id);
	for (i = 0; i < n; i++)
	{
		grn_id valueID = GRN_RECORD_VALUE_AT(values, i);

		if (GRN_RECORD_VALUE(valueMin) == GRN_ID_NIL ||
			GRN_RECORD_VALUE(valueMin) > valueID)
		{
			GRN_RECORD_SET(ctx, valueMin, valueID);
		}

		if (GRN_RECORD_VALUE(valueMax) == GRN_ID_NIL ||
			GRN_RECORD_VALUE(valueMax) < valueID)
		{
			GRN_RECORD_SET(ctx, valueMax, valueID);
		}
	}
}

static void
PGrnJSONValuesDeleteBulk(grn_obj *jsonValuesTable,
						 grn_obj *jsonValuesIndexColumn,
						 grn_obj *valueMin,
						 grn_obj *valueMax)
{
	char minKey[GRN_TABLE_MAX_KEY_SIZE];
	char maxKey[GRN_TABLE_MAX_KEY_SIZE];
	unsigned int minKeySize;
	unsigned int maxKeySize;
	grn_table_cursor *tableCursor;
	grn_id valueID;

	minKeySize = grn_table_get_key(ctx,
								   jsonValuesTable,
								   GRN_RECORD_VALUE(valueMin),
								   minKey,
								   sizeof(minKey));
	maxKeySize = grn_table_get_key(ctx,
								   jsonValuesTable,
								   GRN_RECORD_VALUE(valueMax),
								   maxKey,
								   sizeof(maxKey));
	tableCursor = grn_table_cursor_open(ctx, jsonValuesTable,
										minKey, minKeySize,
										maxKey, maxKeySize,
										0, -1, 0);
	if (!tableCursor)
		return;

	while ((valueID = grn_table_cursor_next(ctx, tableCursor)) != GRN_ID_NIL)
	{
		grn_ii_cursor *iiCursor;
		bool haveReference = false;

		iiCursor = grn_ii_cursor_open(ctx,
									  (grn_ii *)jsonValuesIndexColumn,
									  valueID,
									  GRN_ID_NIL, GRN_ID_MAX, 0, 0);
		if (iiCursor)
		{
			while (grn_ii_cursor_next(ctx, iiCursor))
			{
				haveReference = true;
				break;
			}
			grn_ii_cursor_close(ctx, iiCursor);
		}

		if (!haveReference)
			grn_table_cursor_delete(ctx, tableCursor);
	}

	grn_table_cursor_close(ctx, tableCursor);
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
		grn_obj *sourcesValuesColumn = NULL;
		grn_obj *jsonValuesTable = NULL;
		grn_obj *jsonValuesIndexColumn = NULL;
		grn_obj jsonValues;
		grn_obj jsonValueMin;
		grn_obj jsonValueMax;

		sourcesCtidColumn = PGrnLookupSourcesCtidColumn(index, ERROR);

#ifdef JSONBOID
		{
			TupleDesc desc;
			Form_pg_attribute attribute;

			desc = RelationGetDescr(index);
			attribute = desc->attrs[0];
			if (attribute->atttypid == JSONBOID)
			{
				grn_id jsonValuesTableID;

				sourcesValuesColumn = PGrnLookupColumn(sourcesTable,
													   attribute->attname.data,
													   ERROR);
				jsonValuesTable = PGrnLookupJSONValuesTable(index, 0, ERROR);
				jsonValuesIndexColumn = PGrnLookupColumn(jsonValuesTable,
														 PGrnIndexColumnName,
														 ERROR);

				jsonValuesTableID = grn_obj_id(ctx, jsonValuesTable);
				GRN_RECORD_INIT(&jsonValues,   0, jsonValuesTableID);
				GRN_RECORD_INIT(&jsonValueMin, 0, jsonValuesTableID);
				GRN_RECORD_INIT(&jsonValueMax, 0, jsonValuesTableID);

				GRN_RECORD_SET(ctx, &jsonValueMin, GRN_ID_NIL);
				GRN_RECORD_SET(ctx, &jsonValueMax, GRN_ID_NIL);
			}
		}
#endif

		while ((id = grn_table_cursor_next(ctx, cursor)) != GRN_ID_NIL)
		{
			ItemPointerData	ctid;

			CHECK_FOR_INTERRUPTS();

			GRN_BULK_REWIND(&ctidBuffer);
			grn_obj_get_value(ctx, sourcesCtidColumn, id, &ctidBuffer);
			ctid = UInt64ToCtid(GRN_UINT64_VALUE(&ctidBuffer));
			if (callback(&ctid, callback_state))
			{
				if (jsonValuesTable)
				{
					GRN_BULK_REWIND(&jsonValues);
					grn_obj_get_value(ctx, sourcesValuesColumn, id, &jsonValues);
					PGrnJSONValuesUpdateDeletedID(jsonValuesTable,
												  &jsonValues,
												  &jsonValueMin,
												  &jsonValueMax);
				}

				grn_table_cursor_delete(ctx, cursor);

				nRemovedTuples += 1;
			}
		}

		if (jsonValuesTable)
		{
			PGrnJSONValuesDeleteBulk(jsonValuesTable,
									 jsonValuesIndexColumn,
									 &jsonValueMin,
									 &jsonValueMax);

			GRN_OBJ_FIN(ctx, &jsonValues);
			grn_obj_unlink(ctx, sourcesValuesColumn);
			grn_obj_unlink(ctx, jsonValuesIndexColumn);
			grn_obj_unlink(ctx, jsonValuesTable);
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

static bool
PGrnRemoveObject(const char *name)
{
	grn_obj *object = grn_ctx_get(ctx, name, strlen(name));

	if (object)
	{
		grn_obj_remove(ctx, object);
		return true;
	}
	else
	{
		return false;
	}
}

#ifdef JSONBOID
static bool
PGrnRemoveJSONValueLexiconTable(const char *typeName, unsigned int relationID)
{
	char tableName[GRN_TABLE_MAX_KEY_SIZE];
	snprintf(tableName, sizeof(tableName),
			 PGrnJSONValueLexiconNameFormat,
			 typeName, relationID, 0);
	return PGrnRemoveObject(tableName);
}
#endif

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
			snprintf(tableName, sizeof(tableName),
					 PGrnLexiconNameFormat, relationID, i);
			if (!PGrnRemoveObject(tableName))
				break;
		}

		{
			char tableName[GRN_TABLE_MAX_KEY_SIZE];
			snprintf(tableName, sizeof(tableName),
					 PGrnSourcesTableNameFormat, relationID);
			PGrnRemoveObject(tableName);
		}

#ifdef JSONBOID
		PGrnRemoveJSONValueLexiconTable("FullTextSearch", relationID);
		PGrnRemoveJSONValueLexiconTable("String", relationID);
		PGrnRemoveJSONValueLexiconTable("Number", relationID);
		PGrnRemoveJSONValueLexiconTable("Boolean", relationID);
		PGrnRemoveJSONValueLexiconTable("Size", relationID);

		{
			char name[GRN_TABLE_MAX_KEY_SIZE];

			snprintf(name, sizeof(name),
					 PGrnJSONPathsTableNameFormat ".%s",
					 relationID, 0, PGrnIndexColumnName);
			PGrnRemoveObject(name);

			snprintf(name, sizeof(name),
					 PGrnJSONValuesTableNameFormat,
					 relationID, 0);
			PGrnRemoveObject(name);

			snprintf(name, sizeof(name),
					 PGrnJSONPathsTableNameFormat,
					 relationID, 0);
			PGrnRemoveObject(name);

			snprintf(name, sizeof(name),
					 PGrnJSONTypesTableNameFormat,
					 relationID, 0);
			PGrnRemoveObject(name);
		}
#endif
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
