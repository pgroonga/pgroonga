#include "pgroonga.h"
#include "pgrn-compatible.h"
#include "pgrn-portable.h"

#include "pgrn-crash-safer-statuses.h"
#include "pgrn-database.h"
#include "pgrn-file.h"
#include "pgrn-value.h"

#include <access/heapam.h>
#ifdef PGRN_SUPPORT_TABLEAM
#	include <access/tableam.h>
#else
#	include <access/htup_details.h>
#endif
#include <access/xact.h>
#include <catalog/pg_database.h>
#include <executor/spi.h>
#include <miscadmin.h>
#include <pgstat.h>
#include <postmaster/bgworker.h>
#include <storage/ipc.h>
#include <storage/latch.h>
#include <storage/procsignal.h>
#include <utils/snapmgr.h>
#include <utils/guc.h>
#include <utils/snapmgr.h>
#include <utils/timestamp.h>

#include <groonga.h>


/* #define PGROONGA_CRASH_SAFER_DEBUG */
#ifdef PGROONGA_CRASH_SAFER_DEBUG
#	define P(...) ereport(LOG, (errmsg(TAG __VA_ARGS__)))
#else
#	define P(...)
#endif

PG_MODULE_MAGIC;

extern PGDLLEXPORT void _PG_init(void);
extern PGDLLEXPORT void
pgroonga_crash_safer_reindex_one(Datum datum) pg_attribute_noreturn();
extern PGDLLEXPORT void
pgroonga_crash_safer_flush_one(Datum datum) pg_attribute_noreturn();
extern PGDLLEXPORT void
pgroonga_crash_safer_main(Datum datum) pg_attribute_noreturn();

#define TAG "pgroonga: crash-safer"

static volatile sig_atomic_t PGroongaCrashSaferGotSIGTERM = false;
static volatile sig_atomic_t PGroongaCrashSaferGotSIGHUP = false;
static volatile sig_atomic_t PGroongaCrashSaferGotSIGUSR1 = false;
static int PGroongaCrashSaferFlushNaptime = 60;
static char *PGroongaCrashSaferLogPath;
static int PGroongaCrashSaferLogLevel;
PGRN_DEFINE_LOG_LEVEL_ENTRIES(PGroongaCrashSaferLogLevelEntries);
static const char *PGroongaCrashSaferLibraryName = "pgroonga_crash_safer";


#if PG_VERSION_NUM < 140000
/* Borrowed from src/backend/utils/adt/timestamp.c in PostgreSQL.
 *
 * Portions Copyright (c) 1996-2022, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 */
static inline long
PGrnTimestampDifferenceMilliseconds(TimestampTz start_time,
									TimestampTz stop_time)
{
	TimestampTz diff = stop_time - start_time;

	if (diff <= 0)
		return 0;
	else
		return (long) ((diff + 999) / 1000);
}
#else
#	define PGrnTimestampDifferenceMilliseconds(start_time, stop_time)	\
	TimestampDifferenceMilliseconds((start_time), (stop_time))
#endif


static uint32_t
pgroonga_crash_safer_get_thread_limit(void *data)
{
	return 1;
}

static void
pgroonga_crash_safer_sigterm(SIGNAL_ARGS)
{
	int	save_errno = errno;

	PGroongaCrashSaferGotSIGTERM = true;
	SetLatch(MyLatch);

	errno = save_errno;
}

static void
pgroonga_crash_safer_sighup(SIGNAL_ARGS)
{
	int	save_errno = errno;

	PGroongaCrashSaferGotSIGHUP = true;
	SetLatch(MyLatch);

	errno = save_errno;
}

static void
pgroonga_crash_safer_sigusr1(SIGNAL_ARGS)
{
	procsignal_sigusr1_handler(postgres_signal_arg);

	PGroongaCrashSaferGotSIGUSR1 = true;
}

void
pgroonga_crash_safer_reindex_one(Datum databaseInfoDatum)
{
	uint64 databaseInfo = DatumGetUInt64(databaseInfoDatum);
	Oid databaseOid;
	Oid tableSpaceOid;

	PGRN_DATABASE_INFO_UNPACK(databaseInfo, databaseOid, tableSpaceOid);

	PGrnBackgroundWorkerInitializeConnectionByOid(databaseOid, InvalidOid, 0);

	StartTransactionCommand();
	SPI_connect();
	PushActiveSnapshot(GetTransactionSnapshot());
	pgstat_report_activity(STATE_RUNNING, TAG ": reindexing");

	{
		int result;
		StringInfoData buffer;
		uint64 i;

		SetCurrentStatementStartTimestamp();
		result = SPI_execute("SELECT (namespace.nspname || "
							 "        '.' || "
							 "        class.relname) AS index_name "
							 "  FROM pg_catalog.pg_class AS class "
							 "  JOIN pg_catalog.pg_namespace AS namespace "
							 "    ON class.relnamespace = namespace.oid "
							 " WHERE class.relam = ("
							 "   SELECT oid "
							 "     FROM pg_catalog.pg_am "
							 "    WHERE amname = 'pgroonga'"
							 " )"
							 "ORDER BY "
							 "  CASE "
							 "  WHEN array_to_string(class.reloptions, ' ', ' ') "
							 "       LIKE '%${%}%' "
							 "    THEN 1 "
							 "  ELSE 0 "
							 "  END, "
							 "  class.relname",
							 true,
							 0);
		if (result != SPI_OK_SELECT)
		{
			ereport(FATAL,
					(errmsg(TAG ": failed to detect PGroonga indexes: "
							"%u/%u: %d",
							databaseOid,
							tableSpaceOid,
							result)));
		}

		initStringInfo(&buffer);
		for (i = 0; i < SPI_processed; i++)
		{
			bool isNull;
			Datum indexName;
			bool readOnly;

			indexName = SPI_getbinval(SPI_tuptable->vals[0],
									  SPI_tuptable->tupdesc,
									  i + 1,
									  &isNull);
			resetStringInfo(&buffer);
			appendStringInfo(&buffer,
							 "REINDEX INDEX %.*s",
							 (int) VARSIZE_ANY_EXHDR(indexName),
							 VARDATA_ANY(indexName));
			SetCurrentStatementStartTimestamp();
#if PG_VERSION_NUM >= 140000
			readOnly = false;
#else
			/* Blocked with readOnly = false */
			readOnly = true;
#endif
			result = SPI_execute(buffer.data, readOnly, 0);
			if (result != SPI_OK_UTILITY)
			{
				ereport(FATAL,
						(errmsg(TAG ": failed to reindex PGroonga index: "
								"%u/%u: <%.*s>: %d",
								databaseOid,
								tableSpaceOid,
								(int) VARSIZE_ANY_EXHDR(indexName),
								VARDATA_ANY(indexName),
								result)));
			}
		}
	}

	PopActiveSnapshot();
	SPI_finish();
	CommitTransactionCommand();

	pgstat_report_activity(STATE_IDLE, NULL);

	proc_exit(0);
}

static void
pgroonga_crash_safer_flush_one_remove_pid_on_exit(int code,
												  Datum databaseInfoDatum)
{
	uint64 databaseInfo = DatumGetUInt64(databaseInfoDatum);
	Oid databaseOid;
	Oid tableSpaceOid;
	bool found;
	pgrn_crash_safer_statuses_entry *entry;
	PGRN_DATABASE_INFO_UNPACK(databaseInfo, databaseOid, tableSpaceOid);
	entry = pgrn_crash_safer_statuses_search(NULL,
											 databaseOid,
											 tableSpaceOid,
											 HASH_FIND,
											 &found);
	if (!found)
		return;
	entry->pid = 0;
}

static void
pgroonga_crash_safer_flush_one_on_exit(int code, Datum databaseInfoDatum)
{
	uint64 databaseInfo = DatumGetUInt64(databaseInfoDatum);
	Oid databaseOid;
	Oid tableSpaceOid;
	PGRN_DATABASE_INFO_UNPACK(databaseInfo, databaseOid, tableSpaceOid);
	pgrn_crash_safer_statuses_stop(NULL, databaseOid, tableSpaceOid);
}

void
pgroonga_crash_safer_flush_one(Datum databaseInfoDatum)
{
	uint64 databaseInfo = DatumGetUInt64(databaseInfoDatum);
	Oid databaseOid;
	Oid tableSpaceOid;
	char *databasePath;
	char pgrnDatabasePath[MAXPGPATH];
	bool pgrnDatabasePathExist;
	bool needReindex = false;
	grn_ctx ctx;
	grn_obj *db;
	HTAB *statuses;
	TimestampTz lastFlushTime = GetCurrentTimestamp();

	before_shmem_exit(pgroonga_crash_safer_flush_one_remove_pid_on_exit,
					  databaseInfoDatum);

	pqsignal(SIGTERM, pgroonga_crash_safer_sigterm);
	pqsignal(SIGHUP, pgroonga_crash_safer_sighup);
	pqsignal(SIGUSR1, pgroonga_crash_safer_sigusr1);
	BackgroundWorkerUnblockSignals();

	PGRN_DATABASE_INFO_UNPACK(databaseInfo, databaseOid, tableSpaceOid);

	databasePath = GetDatabasePath(databaseOid, tableSpaceOid);
	join_path_components(pgrnDatabasePath,
						 databasePath,
						 PGrnDatabaseBasename);

	P(": flush: %u/%u", databaseOid, tableSpaceOid);

	pgstat_report_activity(STATE_RUNNING, TAG ": flushing");

	grn_thread_set_get_limit_func(pgroonga_crash_safer_get_thread_limit,
								  NULL);
	grn_default_logger_set_flags(grn_default_logger_get_flags() |
								 GRN_LOG_PID);
	grn_default_logger_set_max_level(PGroongaCrashSaferLogLevel);
	if (!PGrnIsNoneValue(PGroongaCrashSaferLogPath))
	{
		grn_default_logger_set_path(PGroongaCrashSaferLogPath);
	}

	if (grn_init() != GRN_SUCCESS)
	{
		ereport(ERROR,
				(errcode(ERRCODE_SYSTEM_ERROR),
				 errmsg(TAG ": failed to initialize Groonga")));
	}

	grn_set_segv_handler();

	if (grn_ctx_init(&ctx, 0) != GRN_SUCCESS)
	{
		ereport(ERROR,
				(errcode(ERRCODE_SYSTEM_ERROR),
				 errmsg(TAG ": failed to initialize Groonga context")));
	}

	GRN_LOG(&ctx,
			GRN_LOG_NOTICE,
			TAG ": initialize: <%s>",
			PGRN_VERSION);

	grn_ctx_set_wal_role(&ctx, GRN_WAL_ROLE_PRIMARY);

	pgrnDatabasePathExist = pgrn_file_exist(pgrnDatabasePath);
	if (pgrnDatabasePathExist)
	{
		db = grn_db_open(&ctx, pgrnDatabasePath);
	}
	else
	{
		db = grn_db_create(&ctx, pgrnDatabasePath, NULL);
	}
	if (!db)
	{
		GRN_LOG(&ctx,
				GRN_LOG_WARNING,
				TAG ": failed to %s database: <%s>",
				pgrnDatabasePathExist ? "open" : "create",
				pgrnDatabasePath);
		PGrnDatabaseRemoveAllRelatedFiles(databasePath);
		db = grn_db_create(&ctx, pgrnDatabasePath, NULL);
		if (!db)
		{
			ereport(ERROR,
					(errcode(ERRCODE_SYSTEM_ERROR),
					 errmsg(TAG ": failed to recreate Groonga database: %s",
							ctx.errbuf)));
		}
		needReindex = true;
	}
	pfree(databasePath);

	statuses = pgrn_crash_safer_statuses_get();
	pgrn_crash_safer_statuses_start(statuses, databaseOid, tableSpaceOid);
	before_shmem_exit(pgroonga_crash_safer_flush_one_on_exit,
					  databaseInfoDatum);

	if (needReindex)
	{
		BackgroundWorker worker = {0};
		BackgroundWorkerHandle *handle;

		snprintf(worker.bgw_name,
				 BGW_MAXLEN,
				 TAG ": reindex: %u/%u",
				 databaseOid,
				 tableSpaceOid);
#ifdef PGRN_BACKGROUND_WORKER_HAVE_BGW_TYPE
		snprintf(worker.bgw_type,
				 BGW_MAXLEN,
				 TAG ": reindex: %u/%u",
				 databaseOid,
				 tableSpaceOid);
#endif
		worker.bgw_flags =
			BGWORKER_SHMEM_ACCESS |
			BGWORKER_BACKEND_DATABASE_CONNECTION;
		worker.bgw_start_time = BgWorkerStart_ConsistentState;
		worker.bgw_restart_time = BGW_NEVER_RESTART;
		snprintf(worker.bgw_library_name,
				 BGW_MAXLEN,
				 "%s", PGroongaCrashSaferLibraryName);
		snprintf(worker.bgw_function_name,
				 BGW_MAXLEN,
				 "pgroonga_crash_safer_reindex_one");
		worker.bgw_main_arg = databaseInfoDatum;
		worker.bgw_notify_pid = MyProcPid;
		if (RegisterDynamicBackgroundWorker(&worker, &handle))
		{
			WaitForBackgroundWorkerShutdown(handle);
		}
	}

	while (!PGroongaCrashSaferGotSIGTERM)
	{
		TimestampTz nextFlushTime =
			TimestampTzPlusMilliseconds(
				lastFlushTime,
				PGroongaCrashSaferFlushNaptime * 1000);
		long timeout =
			PGrnTimestampDifferenceMilliseconds(GetCurrentTimestamp(),
												nextFlushTime);
		int conditions;
		if (timeout <= 0)
		{
			conditions = WL_TIMEOUT;
		}
		else
		{
			conditions = WaitLatch(MyLatch,
								   WL_LATCH_SET |
								   WL_TIMEOUT |
								   PGRN_WL_EXIT_ON_PM_DEATH,
								   timeout,
								   PG_WAIT_EXTENSION);
		}
		if (conditions & WL_LATCH_SET)
		{
			ResetLatch(MyLatch);
			CHECK_FOR_INTERRUPTS();
		}

		if (PGroongaCrashSaferGotSIGHUP)
		{
			PGroongaCrashSaferGotSIGHUP = false;
			ProcessConfigFile(PGC_SIGHUP);
		}

		if (PGroongaCrashSaferGotSIGUSR1)
		{
			PGroongaCrashSaferGotSIGUSR1 = false;
		}

		if (!(conditions & WL_TIMEOUT))
		{
			continue;
		}

		lastFlushTime = GetCurrentTimestamp();

		if (!pgrn_file_exist(pgrnDatabasePath))
			break;

		/* TODO: How to implement safe finish on no connection? */
		/*
		if (pgrn_crash_safer_statuses_get_n_using_processing(statuses,
															 databaseOid,
															 tableSpaceOid) == 0)
			break;
		*/

		grn_obj_flush_recursive(&ctx, db);
	}

	grn_obj_close(&ctx, db);

	grn_ctx_fin(&ctx);

	grn_fin();

	pgstat_report_activity(STATE_IDLE, NULL);

	P(": flush: done: %u/%u", databaseOid, tableSpaceOid);

	proc_exit(1);
}

static void
pgroonga_crash_safer_main_on_exit(int code, Datum arg)
{
	pgrn_crash_safer_statuses_set_main_pid(NULL, 0);
}

void
pgroonga_crash_safer_main(Datum arg)
{
	HTAB *statuses;

	pqsignal(SIGTERM, pgroonga_crash_safer_sigterm);
	pqsignal(SIGHUP, pgroonga_crash_safer_sighup);
	pqsignal(SIGUSR1, pgroonga_crash_safer_sigusr1);
	BackgroundWorkerUnblockSignals();

	PGrnBackgroundWorkerInitializeConnection(NULL, NULL, 0);

	statuses = pgrn_crash_safer_statuses_get();
	pgrn_crash_safer_statuses_set_main_pid(statuses, MyProcPid);
	before_shmem_exit(pgroonga_crash_safer_main_on_exit, 0);
	while (!PGroongaCrashSaferGotSIGTERM)
	{
		int conditions;

		conditions = WaitLatch(MyLatch,
							   WL_LATCH_SET |
							   PGRN_WL_EXIT_ON_PM_DEATH,
							   0,
							   PG_WAIT_EXTENSION);
		if (conditions & WL_LATCH_SET)
		{
			ResetLatch(MyLatch);
			CHECK_FOR_INTERRUPTS();
		}

		if (PGroongaCrashSaferGotSIGHUP)
		{
			PGroongaCrashSaferGotSIGHUP = false;
			ProcessConfigFile(PGC_SIGHUP);
		}

		if (PGroongaCrashSaferGotSIGUSR1)
		{
			HASH_SEQ_STATUS status;
			pgrn_crash_safer_statuses_entry *entry;
			PGroongaCrashSaferGotSIGUSR1 = false;
			hash_seq_init(&status, statuses);
			while ((entry = hash_seq_search(&status)))
			{
				BackgroundWorker worker = {0};
				BackgroundWorkerHandle *handle;
				Oid databaseOid;
				Oid tableSpaceOid;

				if (entry->pid != 0)
					continue;
				if (pg_atomic_read_u32(&(entry->nUsingProcesses)) != 1)
					continue;

				PGRN_DATABASE_INFO_UNPACK(entry->key,
										  databaseOid,
										  tableSpaceOid);
				P(": flush: start: %u/%u",
				  databaseOid,
				  tableSpaceOid);
				snprintf(worker.bgw_name,
						 BGW_MAXLEN,
						 TAG ": flush: %u/%u",
						 databaseOid,
						 tableSpaceOid);
#ifdef PGRN_BACKGROUND_WORKER_HAVE_BGW_TYPE
				snprintf(worker.bgw_type,
						 BGW_MAXLEN,
						 TAG ": flush: %u/%u",
						 databaseOid,
						 tableSpaceOid);
#endif
				worker.bgw_flags = BGWORKER_SHMEM_ACCESS;
				worker.bgw_start_time = BgWorkerStart_ConsistentState;
				worker.bgw_restart_time = BGW_NEVER_RESTART;
				snprintf(worker.bgw_library_name,
						 BGW_MAXLEN,
						 "%s", PGroongaCrashSaferLibraryName);
				snprintf(worker.bgw_function_name,
						 BGW_MAXLEN,
						 "pgroonga_crash_safer_flush_one");
				worker.bgw_main_arg = DatumGetUInt64(entry->key);
				worker.bgw_notify_pid = MyProcPid;
				if (!RegisterDynamicBackgroundWorker(&worker, &handle))
					continue;
				WaitForBackgroundWorkerStartup(handle, &(entry->pid));
			}
		}
	}

	proc_exit(1);
}

void
_PG_init(void)
{
	BackgroundWorker worker = {0};

	DefineCustomIntVariable("pgroonga_crash_safer.flush_naptime",
							"Duration between each flush in seconds.",
							"The default is 60 seconds. "
							"It means that PGroonga crash safer tries to "
							"flush all PGroonga enabled databases "
							"per 1 minute.",
							&PGroongaCrashSaferFlushNaptime,
							PGroongaCrashSaferFlushNaptime,
							1,
							INT_MAX,
							PGC_SIGHUP,
							GUC_UNIT_S,
							NULL,
							NULL,
							NULL);

	DefineCustomStringVariable("pgroonga_crash_safer.log_path",
							   "Log path for pgroonga-crash-safer.",
							   "The default is "
							   "\"${PG_DATA}/" PGrnLogPathDefault "\". "
							   "Use \"none\" to disable file output.",
							   &PGroongaCrashSaferLogPath,
							   PGrnLogPathDefault,
							   PGC_USERSET,
							   0,
							   NULL,
							   NULL,
							   NULL);

	DefineCustomEnumVariable("pgroonga_crash_safer.log_level",
							 "Log level for pgroonga-crash-safer.",
							 "Available log levels: "
							 "[none, emergency, alert, critical, "
							 "error, warning, notice, info, debug, dump]. "
							 "The default is notice.",
							 &PGroongaCrashSaferLogLevel,
							 GRN_LOG_DEFAULT_LEVEL,
							 PGroongaCrashSaferLogLevelEntries,
							 PGC_USERSET,
							 0,
							 NULL,
							 NULL,
							 NULL);

	if (!process_shared_preload_libraries_in_progress)
		return;

	snprintf(worker.bgw_name, BGW_MAXLEN, TAG ": main");
#ifdef PGRN_BACKGROUND_WORKER_HAVE_BGW_TYPE
	snprintf(worker.bgw_type, BGW_MAXLEN, TAG);
#endif
	worker.bgw_flags =
		BGWORKER_SHMEM_ACCESS |
		BGWORKER_BACKEND_DATABASE_CONNECTION;
	worker.bgw_start_time = BgWorkerStart_ConsistentState;
	worker.bgw_restart_time = 60;
	snprintf(worker.bgw_library_name, BGW_MAXLEN,
			 "%s", PGroongaCrashSaferLibraryName);
	snprintf(worker.bgw_function_name, BGW_MAXLEN,
			 "pgroonga_crash_safer_main");
	worker.bgw_main_arg = 0;
	worker.bgw_notify_pid = 0;

	RegisterBackgroundWorker(&worker);
}
