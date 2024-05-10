#include <groonga.h>

#define PGRN_TAG "pgroonga: wal-resource-manager"

static grn_ctx PGrnWRMContext;
static grn_ctx *ctx = &PGrnWRMContext;

#include "pgrn-constant.h"
#include "pgrn-file.h"
#include "pgrn-groonga.h"
#include "pgrn-log-level.h"
#include "pgrn-value.h"
#include "pgrn-wal-custom.h"

#include <utils/guc.h>

PG_MODULE_MAGIC;

static grn_ctx PGrnWRMContext;
static char *PGrnWRMLogPath;
static int PGrnWRMLogLevel = GRN_LOG_DEFAULT_LEVEL;
PGRN_DEFINE_LOG_LEVEL_ENTRIES(PGrnWRMLogLevelEntries);

extern PGDLLEXPORT void _PG_init(void);

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
	grn_obj type;
	grn_obj tokenizer;
	grn_obj normalizers;
	grn_obj tokenFilters;
	PGrnWALRecordCreateTable walRecord = {0};
	PGrnWRMRedoData data = {
		.walRecord = &(walRecord.common),
		.db = NULL,
	};
	GRN_TEXT_INIT(&type, GRN_OBJ_DO_SHALLOW_COPY);
	GRN_TEXT_INIT(&tokenizer, GRN_OBJ_DO_SHALLOW_COPY);
	GRN_TEXT_INIT(&normalizers, GRN_OBJ_DO_SHALLOW_COPY);
	GRN_TEXT_INIT(&tokenFilters, GRN_OBJ_DO_SHALLOW_COPY);
	walRecord.type = &type;
	walRecord.tokenizer = &tokenizer;
	walRecord.normalizers = &normalizers;
	walRecord.tokenFilters = &tokenFilters;
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
	}
	PG_END_TRY();
	GRN_OBJ_FIN(ctx, &type);
	GRN_OBJ_FIN(ctx, &tokenizer);
	GRN_OBJ_FIN(ctx, &normalizers);
	GRN_OBJ_FIN(ctx, &tokenFilters);
}

static void
pgrnwrm_redo(XLogReaderState *record)
{
	uint8 info = XLogRecGetInfo(record) & XLR_RMGR_INFO_MASK;
	GRN_LOG(ctx, GRN_LOG_NOTICE, PGRN_TAG ": redo: <%s>", PGRN_VERSION);
	switch (info)
	{
	case PGRN_WAL_RECORD_CREATE_TABLE:
		pgrnwrm_redo_create_table(record);
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
	GRN_LOG(ctx, GRN_LOG_NOTICE, PGRN_TAG ": identify: <%s>", PGRN_VERSION);
	switch (info)
	{
	case PGRN_WAL_RECORD_CREATE_TABLE:
		return "PGROONGA_CREATE_TABLE";
	default:
		return NULL;
	}
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
}

static void
pgrnwrm_cleanup(void)
{
	GRN_LOG(ctx, GRN_LOG_NOTICE, PGRN_TAG ": cleanup: <%s>", PGRN_VERSION);
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

	RegisterCustomRmgr(PGRN_WAL_RESOURCE_MANAGER_ID, &data);
}
