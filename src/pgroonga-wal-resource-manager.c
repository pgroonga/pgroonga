#include <groonga.h>

#define PGRN_TAG "pgroonga: wal-resource-manager"

static grn_ctx PGrnWRMContext;
static grn_ctx *ctx = &PGrnWRMContext;
static grn_obj PGrnInspectBuffer;

#include "pgrn-compatible.h"
#include "pgrn-constant.h"
#include "pgrn-file.h"
#include "pgrn-groonga.h"
#include "pgrn-log-level.h"
#include "pgrn-value.h"
#ifdef PGRN_SUPPORT_WAL_RESOURCE_MANAGER
#	include "pgrn-wal-custom.h"
#endif

#include <utils/guc.h>

PG_MODULE_MAGIC;

static grn_ctx PGrnWRMContext;
static char *PGrnWRMLogPath;
static int PGrnWRMLogLevel = GRN_LOG_DEFAULT_LEVEL;
PGRN_DEFINE_LOG_LEVEL_ENTRIES(PGrnWRMLogLevelEntries);

extern PGDLLEXPORT void _PG_init(void);

#ifdef PGRN_SUPPORT_WAL_RESOURCE_MANAGER
static uint32_t
pgrnwrm_get_thread_limit(void *data)
{
	return 1;
}

typedef struct PGrnWRMRedoData
{
	PGrnWALRecordCommon *walRecord;
	grn_obj *db;
} PGrnWRMRedoData;

static void
pgrnwrm_redo_setup(PGrnWRMRedoData *data)
{
	PGrnWALRecordCommon *walRecord = data->walRecord;
	grn_encoding encoding = PGrnPGEncodingToGrnEncoding(walRecord->dbEncoding);
	char *databasePath;
	char path[MAXPGPATH];
	GRN_CTX_SET_ENCODING(ctx, encoding);
	databasePath = GetDatabasePath(walRecord->dbID, walRecord->dbTableSpaceID);
	join_path_components(path, databasePath, PGrnDatabaseBasename);
	pfree(databasePath);
	if (pgrn_file_exist(path))
	{
		data->db = grn_db_open(ctx, path);
	}
	else
	{
		data->db = grn_db_create(ctx, path, NULL);
	}
}

static void
pgrnwrm_redo_teardown(PGrnWRMRedoData *data)
{
	if (!data->db)
		return;

	grn_obj_close(ctx, data->db);
}

static void
pgrnwrm_redo_create_table(XLogReaderState *record)
{
	PGrnWALRecordRaw raw = {
		.data = XLogRecGetData(record),
		.size = XLogRecGetDataLen(record),
	};
	grn_obj typeName;
	grn_obj tokenizerName;
	grn_obj normalizersName;
	grn_obj tokenFiltersName;
	PGrnWALRecordCreateTable walRecord = {0};
	PGrnWRMRedoData data = {
		.walRecord = &(walRecord.common),
		.db = NULL,
	};
	GRN_TEXT_INIT(&typeName, GRN_OBJ_DO_SHALLOW_COPY);
	GRN_TEXT_INIT(&tokenizerName, GRN_OBJ_DO_SHALLOW_COPY);
	GRN_TEXT_INIT(&normalizersName, GRN_OBJ_DO_SHALLOW_COPY);
	GRN_TEXT_INIT(&tokenFiltersName, GRN_OBJ_DO_SHALLOW_COPY);
	walRecord.type = &typeName;
	walRecord.tokenizer = &tokenizerName;
	walRecord.normalizers = &normalizersName;
	walRecord.tokenFilters = &tokenFiltersName;
	PGrnWALRecordCreateTableRead(ctx, &walRecord, &raw);
	PG_TRY();
	{
		grn_obj *type = NULL;
		pgrnwrm_redo_setup(&data);
		if (GRN_BULK_VSIZE(walRecord.type) > 0)
		{
			type = PGrnLookupWithSize(GRN_TEXT_VALUE(walRecord.type),
									  GRN_TEXT_LEN(walRecord.type),
									  ERROR);
		}
		PGrnCreateTableRawWithSize(walRecord.indexTableSpaceID,
								   walRecord.name,
								   walRecord.nameSize,
								   walRecord.flags,
								   type,
								   walRecord.tokenizer,
								   walRecord.normalizers,
								   walRecord.tokenFilters);
	}
	PG_FINALLY();
	{
		pgrnwrm_redo_teardown(&data);
		GRN_OBJ_FIN(ctx, &typeName);
		GRN_OBJ_FIN(ctx, &tokenizerName);
		GRN_OBJ_FIN(ctx, &normalizersName);
		GRN_OBJ_FIN(ctx, &tokenFiltersName);
	}
	PG_END_TRY();
}

static void
pgrnwrm_redo_create_column(XLogReaderState *record)
{
	PGrnWALRecordRaw raw = {
		.data = XLogRecGetData(record),
		.size = XLogRecGetDataLen(record),
	};
	grn_obj tableName;
	grn_obj typeName;
	PGrnWALRecordCreateColumn walRecord = {0};
	PGrnWRMRedoData data = {
		.walRecord = &(walRecord.common),
		.db = NULL,
	};
	GRN_TEXT_INIT(&tableName, GRN_OBJ_DO_SHALLOW_COPY);
	GRN_TEXT_INIT(&typeName, GRN_OBJ_DO_SHALLOW_COPY);
	walRecord.table = &tableName;
	walRecord.type = &typeName;
	PGrnWALRecordCreateColumnRead(ctx, &walRecord, &raw);
	PG_TRY();
	{
		grn_obj *table = NULL;
		grn_obj *type = NULL;
		pgrnwrm_redo_setup(&data);
		table = PGrnLookupWithSize(GRN_TEXT_VALUE(walRecord.table),
								   GRN_TEXT_LEN(walRecord.table),
								   ERROR);
		type = PGrnLookupWithSize(GRN_TEXT_VALUE(walRecord.type),
								  GRN_TEXT_LEN(walRecord.type),
								  ERROR);
		PGrnCreateColumnRawWithSize(walRecord.indexTableSpaceID,
									table,
									walRecord.name,
									walRecord.nameSize,
									walRecord.flags,
									type);
	}
	PG_FINALLY();
	{
		pgrnwrm_redo_teardown(&data);
		GRN_OBJ_FIN(ctx, &tableName);
		GRN_OBJ_FIN(ctx, &typeName);
	}
	PG_END_TRY();
}

static const char *
pgrnwrm_info_to_string(uint8 info)
{
	switch (info)
	{
	case PGRN_WAL_RECORD_CREATE_TABLE:
		return "PGROONGA_CREATE_TABLE";
	case PGRN_WAL_RECORD_CREATE_COLUMN:
		return "PGROONGA_CREATE_COLUMN";
	default:
		return "PGROONGA_UNKNOWN";
	}
}

static void
pgrnwrm_redo(XLogReaderState *record)
{
	uint8 info = XLogRecGetInfo(record) & XLR_RMGR_INFO_MASK;
	GRN_LOG(ctx,
			GRN_LOG_NOTICE,
			PGRN_TAG ": redo: <%s>(%u): <%s>",
			pgrnwrm_info_to_string(info),
			info,
			PGRN_VERSION);
	switch (info)
	{
	case PGRN_WAL_RECORD_CREATE_TABLE:
		pgrnwrm_redo_create_table(record);
		break;
	case PGRN_WAL_RECORD_CREATE_COLUMN:
		pgrnwrm_redo_create_column(record);
		break;
	default:
		ereport(ERROR,
				errcode(ERRCODE_DATA_EXCEPTION),
				errmsg(PGRN_TAG ": [redo] unknown info %u", info));
		break;
	}
}

static void
pgrnwrm_desc(StringInfo buffer, XLogReaderState *record)
{
	GRN_LOG(ctx, GRN_LOG_NOTICE, PGRN_TAG ": desc: <%s>", PGRN_VERSION);
}

static const char *
pgrnwrm_identify(uint8 info)
{
	GRN_LOG(ctx,
			GRN_LOG_NOTICE,
			PGRN_TAG ": identify: <%u>: <%s>",
			info,
			PGRN_VERSION);
	return pgrnwrm_info_to_string(info);
}

static void
pgrnwrm_startup(void)
{
	grn_thread_set_get_limit_func(pgrnwrm_get_thread_limit, NULL);
	grn_default_logger_set_flags(grn_default_logger_get_flags() | GRN_LOG_PID);
	grn_default_logger_set_max_level(PGrnWRMLogLevel);
	if (!PGrnIsNoneValue(PGrnWRMLogPath))
	{
		grn_default_logger_set_path(PGrnWRMLogPath);
	}

	if (grn_init() != GRN_SUCCESS)
	{
		ereport(ERROR,
				(errcode(ERRCODE_SYSTEM_ERROR),
				 errmsg(PGRN_TAG ": failed to initialize Groonga")));
	}

	grn_set_segv_handler();
	grn_set_abrt_handler();

	{
		grn_rc rc = grn_ctx_init(ctx, 0);
		if (rc != GRN_SUCCESS)
		{
			ereport(
				ERROR,
				(errcode(PGrnGrnRCToPGErrorCode(rc)),
				 errmsg(PGRN_TAG ": failed to initialize Groonga context: %d",
						rc)));
		}
	}

	GRN_LOG(ctx, GRN_LOG_NOTICE, PGRN_TAG ": initialize: <%s>", PGRN_VERSION);

	GRN_TEXT_INIT(&PGrnInspectBuffer, 0);
}

static void
pgrnwrm_cleanup(void)
{
	GRN_LOG(ctx, GRN_LOG_NOTICE, PGRN_TAG ": cleanup: <%s>", PGRN_VERSION);
	GRN_OBJ_FIN(ctx, &PGrnInspectBuffer);
	grn_ctx_fin(ctx);
	grn_fin();
}

static void
pgrnwrm_mask(char *pagedata, BlockNumber block_number)
{
	GRN_LOG(ctx, GRN_LOG_NOTICE, PGRN_TAG ": mask: <%s>", PGRN_VERSION);
}

static void
pgrnwrm_decode(struct LogicalDecodingContext *context,
			   struct XLogRecordBuffer *buffer)
{
	GRN_LOG(ctx, GRN_LOG_NOTICE, PGRN_TAG ": decode: <%s>", PGRN_VERSION);
}

static RmgrData data = {
	.rm_name = "PGroonga",
	.rm_redo = pgrnwrm_redo,
	.rm_desc = pgrnwrm_desc,
	.rm_identify = pgrnwrm_identify,
	.rm_startup = pgrnwrm_startup,
	.rm_cleanup = pgrnwrm_cleanup,
	.rm_mask = pgrnwrm_mask,
	.rm_decode = pgrnwrm_decode,
};
#endif

void
_PG_init(void)
{
	DefineCustomStringVariable("pgroonga_wal_resource_manager.log_path",
							   "Log path for pgroonga-wal-resource-manager.",
							   "The default is "
							   "\"${PG_DATA}/" PGrnLogPathDefault "\". "
							   "Use \"none\" to disable file output.",
							   &PGrnWRMLogPath,
							   PGrnLogPathDefault,
							   PGC_USERSET,
							   0,
							   NULL,
							   NULL,
							   NULL);

	DefineCustomEnumVariable("pgroonga_wal_resource_manager.log_level",
							 "Log level for pgroonga-wal-resource-manager.",
							 "Available log levels: "
							 "[none, emergency, alert, critical, "
							 "error, warning, notice, info, debug, dump]. "
							 "The default is notice.",
							 &PGrnWRMLogLevel,
							 GRN_LOG_DEFAULT_LEVEL,
							 PGrnWRMLogLevelEntries,
							 PGC_USERSET,
							 0,
							 NULL,
							 NULL,
							 NULL);

#ifdef PGRN_SUPPORT_WAL_RESOURCE_MANAGER
	RegisterCustomRmgr(PGRN_WAL_RESOURCE_MANAGER_ID, &data);
#endif
}
