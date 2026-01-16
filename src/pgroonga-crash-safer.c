#include <groonga.h>

#define PGRN_TAG "pgroonga: crash-safer"

static grn_ctx *ctx = NULL;

#include "pgrn-compatible.h"
#include "pgrn-portable.h"

#include "pgrn-crash-safer-statuses.h"
#include "pgrn-database.h"
#include "pgrn-file.h"
#include "pgrn-log-level.h"
#include "pgrn-value.h"
#ifdef PGRN_SUPPORT_WAL_RESOURCE_MANAGER
#	include "pgrn-wal-custom.h"
#endif

#include <access/heapam.h>
#include <access/tableam.h>
#include <access/xact.h>
#include <access/xlog.h>
#include <catalog/pg_database.h>
#include <executor/spi.h>
#include <miscadmin.h>
#include <pgstat.h>
#include <postmaster/bgworker.h>
#include <storage/ipc.h>
#include <storage/latch.h>
#include <storage/procsignal.h>
#include <utils/guc.h>
#include <utils/snapmgr.h>
#include <utils/timestamp.h>

#include <groonga.h>

/* #define PGROONGA_CRASH_SAFER_DEBUG */
#ifdef PGROONGA_CRASH_SAFER_DEBUG
#	define P(...) ereport(LOG, (errmsg(PGRN_TAG __VA_ARGS__)))
#else
#	define P(...)
#endif

PG_MODULE_MAGIC;

extern PGDLLEXPORT void _PG_init(void);
extern PGDLLEXPORT pg_noreturn void
pgroonga_crash_safer_reset_position_one(Datum datum) pg_attribute_noreturn();
extern PGDLLEXPORT pg_noreturn void
pgroonga_crash_safer_reindex_one(Datum datum) pg_attribute_noreturn();
extern PGDLLEXPORT pg_noreturn void pgroonga_crash_safer_flush_one(Datum datum)
	pg_attribute_noreturn();
extern PGDLLEXPORT pg_noreturn void pgroonga_crash_safer_main(Datum datum)
	pg_attribute_noreturn();

static volatile sig_atomic_t PGroongaCrashSaferGotSIGTERM = false;
static volatile sig_atomic_t PGroongaCrashSaferGotSIGHUP = false;
static volatile sig_atomic_t PGroongaCrashSaferGotSIGUSR1 = false;
static int PGroongaCrashSaferFlushNaptime = 60;
static char *PGroongaCrashSaferLogPath;
static int PGroongaCrashSaferLogLevel = GRN_LOG_DEFAULT_LEVEL;
static int PGroongaCrashSaferMaxRecoveryThreads = 0;
PGRN_DEFINE_LOG_LEVEL_ENTRIES(PGroongaCrashSaferLogLevelEntries);
static const char *PGroongaCrashSaferLibraryName = "pgroonga_crash_safer";

static uint32_t
pgroonga_crash_safer_get_thread_limit(void *data)
{
	return 1;
}

static void
pgroonga_crash_safer_sigterm(SIGNAL_ARGS)
{
	int save_errno = errno;

	PGroongaCrashSaferGotSIGTERM = true;
	SetLatch(MyLatch);

	errno = save_errno;
}

static void
pgroonga_crash_safer_sighup(SIGNAL_ARGS)
{
	int save_errno = errno;

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

static void
pgroonga_crash_safer_prepare_one_on_exit(int code, Datum databaseInfoDatum)
{
	uint64 databaseInfo = DatumGetUInt64(databaseInfoDatum);
	Oid databaseOid;
	Oid tableSpaceOid;
	PGRN_DATABASE_INFO_UNPACK(databaseInfo, databaseOid, tableSpaceOid);
	pgrn_crash_safer_statuses_set_prepare_pid(
		NULL, databaseOid, tableSpaceOid, InvalidPid);
}

void
pgroonga_crash_safer_reset_position_one(Datum databaseInfoDatum)
{
	uint64 databaseInfo = DatumGetUInt64(databaseInfoDatum);
	Oid databaseOid;
	Oid tableSpaceOid;

	PGRN_DATABASE_INFO_UNPACK(databaseInfo, databaseOid, tableSpaceOid);

	BackgroundWorkerInitializeConnectionByOid(databaseOid, InvalidOid, 0);

	StartTransactionCommand();
	SPI_connect();
	PushActiveSnapshot(GetTransactionSnapshot());
	pgstat_report_activity(STATE_RUNNING, PGRN_TAG ": resetting position");

	pgrn_crash_safer_statuses_set_prepare_pid(
		NULL, databaseOid, tableSpaceOid, MyProcPid);
	before_shmem_exit(pgroonga_crash_safer_prepare_one_on_exit,
					  databaseInfoDatum);

	{
		int result;

		SetCurrentStatementStartTimestamp();
		result = SPI_execute(
			"SELECT nspname "
			"  FROM pg_catalog.pg_namespace "
			"  WHERE oid in ("
			"    SELECT pronamespace "
			"    FROM pg_catalog.pg_proc "
			"    WHERE proname = 'pgroonga_wal_set_applied_position'"
			")",
			true,
			0);
		if (result != SPI_OK_SELECT)
		{
			ereport(FATAL,
					(errmsg(PGRN_TAG ": failed to detect "
									 "pgroonga_wal_set_applied_position(): "
									 "%u/%u: %d",
							databaseOid,
							tableSpaceOid,
							result)));
		}

		if (SPI_processed > 0)
		{
			bool isNULL;
			Datum schemaNameDatum;
			StringInfoData walSetAppliedPosition;

			SetCurrentStatementStartTimestamp();
			/**
			 * The nspname column in pg_catalog.pg_namespace must not be NULL
			 * because of NOT NULL constraint.
			 */
			schemaNameDatum = SPI_getbinval(
				SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 1, &isNULL);
			initStringInfo(&walSetAppliedPosition);
			appendStringInfo(&walSetAppliedPosition,
							 "SELECT %s.pgroonga_wal_set_applied_position()",
							 DatumGetCString(schemaNameDatum));
			result = SPI_execute(walSetAppliedPosition.data, false, 0);
			resetStringInfo(&walSetAppliedPosition);
			if (result != SPI_OK_SELECT)
			{
				ereport(
					FATAL,
					(errmsg(PGRN_TAG ": failed to reset WAL applied positions "
									 "of all PGroonga indexes: "
									 "%u/%u: %d",
							databaseOid,
							tableSpaceOid,
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

void
pgroonga_crash_safer_reindex_one(Datum databaseInfoDatum)
{
	uint64 databaseInfo = DatumGetUInt64(databaseInfoDatum);
	Oid databaseOid;
	Oid tableSpaceOid;

	PGRN_DATABASE_INFO_UNPACK(databaseInfo, databaseOid, tableSpaceOid);

	BackgroundWorkerInitializeConnectionByOid(databaseOid, InvalidOid, 0);

	StartTransactionCommand();
	SPI_connect();
	PushActiveSnapshot(GetTransactionSnapshot());
	pgstat_report_activity(STATE_RUNNING, PGRN_TAG ": reindexing");

	pgrn_crash_safer_statuses_set_prepare_pid(
		NULL, databaseOid, tableSpaceOid, MyProcPid);
	before_shmem_exit(pgroonga_crash_safer_prepare_one_on_exit,
					  databaseInfoDatum);

	{
		int result;
		StringInfoData buffer;
		uint64 i;
		uint64 nIndexes;
		char **indexNames;

		SetCurrentStatementStartTimestamp();
		result =
			SPI_execute("SELECT (namespace.nspname || "
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
					(errmsg(PGRN_TAG ": failed to detect PGroonga indexes: "
									 "%u/%u: %d",
							databaseOid,
							tableSpaceOid,
							result)));
		}

		initStringInfo(&buffer);
		nIndexes = SPI_processed;
		indexNames = palloc(sizeof(char *) * nIndexes);
		for (i = 0; i < nIndexes; i++)
		{
			bool isNull;
			Datum indexName;

			indexName = SPI_getbinval(
				SPI_tuptable->vals[i], SPI_tuptable->tupdesc, 1, &isNull);
			if (isNull)
			{
				indexNames[i] = NULL;
			}
			else
			{
				indexNames[i] =
					pnstrdup(VARDATA_ANY(DatumGetPointer(indexName)),
							 VARSIZE_ANY_EXHDR(DatumGetPointer(indexName)));
			}
		}

		for (i = 0; i < nIndexes; i++)
		{
			if (!indexNames[i])
				continue;

			resetStringInfo(&buffer);
			appendStringInfo(&buffer,
							 "SELECT pgroonga_command('log_put', "
							 "ARRAY["
							 "'level', 'notice', "
							 "'message', '%s: reindexing: %s: %u/%u'"
							 "])",
							 PGRN_TAG,
							 indexNames[i],
							 databaseOid,
							 tableSpaceOid);
			SetCurrentStatementStartTimestamp();
			SPI_execute(buffer.data, false, 0);

			resetStringInfo(&buffer);
			appendStringInfo(&buffer, "REINDEX INDEX %s", indexNames[i]);
			SetCurrentStatementStartTimestamp();
			result = SPI_execute(buffer.data, false, 0);
			if (result != SPI_OK_UTILITY)
			{
				ereport(FATAL,
						(errmsg(PGRN_TAG ": failed to reindex PGroonga index: "
										 "%u/%u: <%s>: %d",
								databaseOid,
								tableSpaceOid,
								indexNames[i],
								result)));
			}

			resetStringInfo(&buffer);
			appendStringInfo(&buffer,
							 "SELECT pgroonga_command('log_put', "
							 "ARRAY["
							 "'level', 'notice', "
							 "'message', '%s: reindexed: %s: %u/%u'"
							 "])",
							 PGRN_TAG,
							 indexNames[i],
							 databaseOid,
							 tableSpaceOid);
			SetCurrentStatementStartTimestamp();
			SPI_execute(buffer.data, false, 0);

			pfree(indexNames[i]);
			indexNames[i] = NULL;
		}
		pfree(indexNames);
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
	entry = pgrn_crash_safer_statuses_search(
		NULL, databaseOid, tableSpaceOid, HASH_FIND, &found);
	if (!found)
		return;
	entry->pid = InvalidPid;
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
	/* Only on the primary. */
	bool needResetPosition = !RecoveryInProgress();
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
	join_path_components(pgrnDatabasePath, databasePath, PGrnDatabaseBasename);

	P(": flush: %u/%u", databaseOid, tableSpaceOid);

	pgstat_report_activity(STATE_RUNNING, PGRN_TAG ": flushing");

	grn_thread_set_get_limit_func(pgroonga_crash_safer_get_thread_limit, NULL);
	grn_default_logger_set_flags(grn_default_logger_get_flags() | GRN_LOG_PID);
	grn_default_logger_set_max_level(PGroongaCrashSaferLogLevel);
	if (!PGrnIsNoneValue(PGroongaCrashSaferLogPath))
	{
		grn_default_logger_set_path(PGroongaCrashSaferLogPath);
	}
	grn_set_default_n_workers(PGroongaCrashSaferMaxRecoveryThreads);

	if (grn_init() != GRN_SUCCESS)
	{
		ereport(ERROR,
				(errcode(ERRCODE_SYSTEM_ERROR),
				 errmsg(PGRN_TAG ": failed to initialize Groonga")));
	}

	grn_set_segv_handler();
	grn_set_abrt_handler();

	if (grn_ctx_init(&ctx, 0) != GRN_SUCCESS)
	{
		ereport(ERROR,
				(errcode(ERRCODE_SYSTEM_ERROR),
				 errmsg(PGRN_TAG ": failed to initialize Groonga context")));
	}

	GRN_LOG(&ctx,
			GRN_LOG_NOTICE,
			PGRN_TAG ": initialize: <%s>: %u/%u",
			PGRN_VERSION,
			databaseOid,
			tableSpaceOid);
	GRN_LOG(&ctx,
			GRN_LOG_DEBUG,
			PGRN_TAG ": max_recovery_threads: %d",
			grn_get_default_n_workers());

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
				PGRN_TAG ": failed to %s database: <%s>",
				pgrnDatabasePathExist ? "open" : "create",
				pgrnDatabasePath);
		PGrnDatabaseRemoveAllRelatedFiles(databasePath);
		db = grn_db_create(&ctx, pgrnDatabasePath, NULL);
		if (!db)
		{
			ereport(
				ERROR,
				(errcode(ERRCODE_SYSTEM_ERROR),
				 errmsg(PGRN_TAG ": failed to recreate Groonga database: %s",
						ctx.errbuf)));
		}
		needReindex = true;
	}
	pfree(databasePath);

	if (needReindex || needResetPosition)
	{
		BackgroundWorker worker = {0};
		BackgroundWorkerHandle *handle;

		GRN_LOG(&ctx,
				GRN_LOG_NOTICE,
				PGRN_TAG ": %s: %u/%u",
				needReindex ? "reindexing" : "resetting-position",
				databaseOid,
				tableSpaceOid);

		snprintf(worker.bgw_name,
				 BGW_MAXLEN,
				 PGRN_TAG ": prepare: %s: %u/%u",
				 needReindex ? "reindex" : "reset-position",
				 databaseOid,
				 tableSpaceOid);
		snprintf(worker.bgw_type, BGW_MAXLEN, "%s", worker.bgw_name);
		worker.bgw_flags =
			BGWORKER_SHMEM_ACCESS | BGWORKER_BACKEND_DATABASE_CONNECTION;
		worker.bgw_start_time = BgWorkerStart_ConsistentState;
		worker.bgw_restart_time = BGW_NEVER_RESTART;
		snprintf(worker.bgw_library_name,
				 BGW_MAXLEN,
				 "%s",
				 PGroongaCrashSaferLibraryName);
		snprintf(worker.bgw_function_name,
				 BGW_MAXLEN,
				 needReindex ? "pgroonga_crash_safer_reindex_one"
							 : "pgroonga_crash_safer_reset_position_one");
		worker.bgw_main_arg = databaseInfoDatum;
		worker.bgw_notify_pid = MyProcPid;
		if (RegisterDynamicBackgroundWorker(&worker, &handle))
		{
			WaitForBackgroundWorkerShutdown(handle);
			GRN_LOG(&ctx,
					GRN_LOG_NOTICE,
					PGRN_TAG ": %s: %u/%u",
					needReindex ? "reindexed" : "reset-position",
					databaseOid,
					tableSpaceOid);
		}
	}

	GRN_LOG(&ctx,
			GRN_LOG_NOTICE,
			PGRN_TAG ": ready: %u/%u",
			databaseOid,
			tableSpaceOid);

	statuses = pgrn_crash_safer_statuses_get();
	pgrn_crash_safer_statuses_start(statuses, databaseOid, tableSpaceOid);
	before_shmem_exit(pgroonga_crash_safer_flush_one_on_exit,
					  databaseInfoDatum);

	while (!PGroongaCrashSaferGotSIGTERM)
	{
		TimestampTz nextFlushTime = TimestampTzPlusMilliseconds(
			lastFlushTime, PGroongaCrashSaferFlushNaptime * 1000);
		long timeout = TimestampDifferenceMilliseconds(GetCurrentTimestamp(),
													   nextFlushTime);
		int conditions;
		if (timeout <= 0)
		{
			conditions = WL_TIMEOUT;
		}
		else
		{
			conditions =
				WaitLatch(MyLatch,
						  WL_LATCH_SET | WL_TIMEOUT | WL_EXIT_ON_PM_DEATH,
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
		if (pgrn_crash_safer_statuses_get_n_using_processes(statuses,
															databaseOid,
															tableSpaceOid) == 0)
			break;
		*/

		grn_obj_flush_recursive(&ctx, db);
	}

	PGroongaCrashSaferGotSIGTERM = false;
	while (!PGroongaCrashSaferGotSIGTERM && pgrn_file_exist(pgrnDatabasePath))
	{
		int conditions;
		uint32 n_using_processes =
			pgrn_crash_safer_statuses_get_n_using_processes(
				statuses, databaseOid, tableSpaceOid);
		if (n_using_processes == 0)
			break;

		GRN_LOG(&ctx,
				GRN_LOG_NOTICE,
				PGRN_TAG ": waiting for connections to finish: %u: %u/%u",
				n_using_processes,
				databaseOid,
				tableSpaceOid);
		conditions = WaitLatch(MyLatch,
							   WL_LATCH_SET | WL_TIMEOUT | WL_EXIT_ON_PM_DEATH,
							   PGroongaCrashSaferFlushNaptime * 1000,
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
	}

	if (pgrn_file_exist(pgrnDatabasePath))
	{
		GRN_LOG(&ctx,
				GRN_LOG_NOTICE,
				PGRN_TAG ": flushing database before closing: %u/%u",
				databaseOid,
				tableSpaceOid);
		grn_obj_flush_recursive(&ctx, db);
		GRN_LOG(&ctx,
				GRN_LOG_NOTICE,
				PGRN_TAG ": flushed database before closing: %u/%u",
				databaseOid,
				tableSpaceOid);
	}

	GRN_LOG(&ctx,
			GRN_LOG_NOTICE,
			PGRN_TAG ": closing database: %u/%u",
			databaseOid,
			tableSpaceOid);
	grn_obj_close(&ctx, db);
	GRN_LOG(&ctx,
			GRN_LOG_NOTICE,
			PGRN_TAG ": closed database: %u/%u",
			databaseOid,
			tableSpaceOid);

	grn_ctx_fin(&ctx);

	grn_fin();

	pgstat_report_activity(STATE_IDLE, NULL);

	P(": flush: done: %u/%u", databaseOid, tableSpaceOid);

	proc_exit(0);
}

static void
pgroonga_crash_safer_main_flush_one(pgrn_crash_safer_statuses_entry *entry)
{
	BackgroundWorker worker = {0};
	BackgroundWorkerHandle *handle;
	Oid databaseOid;
	Oid tableSpaceOid;

	PGRN_DATABASE_INFO_UNPACK(entry->key, databaseOid, tableSpaceOid);
	P(": flush: start: %u/%u", databaseOid, tableSpaceOid);
	snprintf(worker.bgw_name,
			 BGW_MAXLEN,
			 PGRN_TAG ": flush: %u/%u",
			 databaseOid,
			 tableSpaceOid);
	snprintf(worker.bgw_type, BGW_MAXLEN, "%s", worker.bgw_name);
	worker.bgw_flags = BGWORKER_SHMEM_ACCESS;
	worker.bgw_start_time = BgWorkerStart_ConsistentState;
	worker.bgw_restart_time = BGW_NEVER_RESTART;
	snprintf(worker.bgw_library_name,
			 BGW_MAXLEN,
			 "%s",
			 PGroongaCrashSaferLibraryName);
	snprintf(
		worker.bgw_function_name, BGW_MAXLEN, "pgroonga_crash_safer_flush_one");
	worker.bgw_main_arg = DatumGetUInt64(entry->key);
	worker.bgw_notify_pid = MyProcPid;
	if (!RegisterDynamicBackgroundWorker(&worker, &handle))
		return;
	WaitForBackgroundWorkerStartup(handle, &(entry->pid));
}

static void
pgroonga_crash_safer_main_flush_all(void)
{
	HTAB *statuses;
	const LOCKMODE lock = AccessShareLock;
	Relation pg_database;
	TableScanDesc scan;
	HeapTuple tuple;

	StartTransactionCommand();
	PushActiveSnapshot(GetTransactionSnapshot());
	pgstat_report_activity(STATE_RUNNING,
						   PGRN_TAG ": start flush process for all databases");

	statuses = pgrn_crash_safer_statuses_get();

	pg_database = table_open(DatabaseRelationId, lock);
	scan = table_beginscan_catalog(pg_database, 0, NULL);
	for (tuple = heap_getnext(scan, ForwardScanDirection);
		 HeapTupleIsValid(tuple);
		 tuple = heap_getnext(scan, ForwardScanDirection))
	{
		Form_pg_database form = (Form_pg_database) GETSTRUCT(tuple);
		Oid databaseOid;
		Oid tableSpaceOid;
		char *databasePath;
		char pgrnDatabasePath[MAXPGPATH];
		pgrn_crash_safer_statuses_entry *entry;

		databaseOid = form->oid;
		tableSpaceOid = form->dattablespace;

		databasePath = GetDatabasePath(databaseOid, tableSpaceOid);
		join_path_components(
			pgrnDatabasePath, databasePath, PGrnDatabaseBasename);
		if (!pgrn_file_exist(pgrnDatabasePath))
			continue;

		entry = pgrn_crash_safer_statuses_search(
			statuses, databaseOid, tableSpaceOid, HASH_ENTER, NULL);
		pgroonga_crash_safer_main_flush_one(entry);
	}
	table_endscan(scan);
	table_close(pg_database, lock);

	PopActiveSnapshot();
	CommitTransactionCommand();
	pgstat_report_activity(STATE_IDLE, NULL);
}

static void
pgroonga_crash_safer_main_on_exit(int code, Datum arg)
{
	pgrn_crash_safer_statuses_set_main_pid(NULL, InvalidPid);
}

void
pgroonga_crash_safer_main(Datum arg)
{
	HTAB *statuses;

	pqsignal(SIGTERM, pgroonga_crash_safer_sigterm);
	pqsignal(SIGHUP, pgroonga_crash_safer_sighup);
	pqsignal(SIGUSR1, pgroonga_crash_safer_sigusr1);
	BackgroundWorkerUnblockSignals();

	BackgroundWorkerInitializeConnection(NULL, NULL, 0);

	statuses = pgrn_crash_safer_statuses_get();
	pgrn_crash_safer_statuses_set_main_pid(statuses, MyProcPid);
	before_shmem_exit(pgroonga_crash_safer_main_on_exit, 0);

	pgroonga_crash_safer_main_flush_all();

	while (!PGroongaCrashSaferGotSIGTERM)
	{
		int conditions;

		conditions = WaitLatch(
			MyLatch, WL_LATCH_SET | WL_EXIT_ON_PM_DEATH, 0, PG_WAIT_EXTENSION);
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
				if (entry->pid != InvalidPid)
					continue;
				if (pg_atomic_read_u32(&(entry->nUsingProcesses)) != 1)
					continue;
				pgroonga_crash_safer_main_flush_one(entry);
			}
		}
	}

	proc_exit(0);
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

	DefineCustomIntVariable(
		"pgroonga_crash_safer.max_recovery_threads",
		"Maximum number of threads for recovery of broken Groonga indexes.",
		"The default is 0, which means disabled. "
		"Use all CPUs in the environment at -1. "
		"Use CPU for that number if 1 or later is set.",
		&PGroongaCrashSaferMaxRecoveryThreads,
		PGroongaCrashSaferMaxRecoveryThreads,
		-1,
		INT_MAX,
		PGC_SIGHUP,
		0,
		NULL,
		NULL,
		NULL);

	if (!process_shared_preload_libraries_in_progress)
		return;

#ifdef PGRN_SUPPORT_WAL_RESOURCE_MANAGER
	/* Use pgroonga-wal-resource-manager for crash safe on standby. */
	if (StandbyMode && RmgrIdExists(PGRN_WAL_RESOURCE_MANAGER_ID))
		return;
#endif

	snprintf(worker.bgw_name, BGW_MAXLEN, PGRN_TAG ": main");
	snprintf(worker.bgw_type, BGW_MAXLEN, "%s", worker.bgw_name);
	worker.bgw_flags =
		BGWORKER_SHMEM_ACCESS | BGWORKER_BACKEND_DATABASE_CONNECTION;
	worker.bgw_start_time = BgWorkerStart_ConsistentState;
	worker.bgw_restart_time = 60;
	snprintf(worker.bgw_library_name,
			 BGW_MAXLEN,
			 "%s",
			 PGroongaCrashSaferLibraryName);
	snprintf(worker.bgw_function_name, BGW_MAXLEN, "pgroonga_crash_safer_main");
	worker.bgw_main_arg = 0;
	worker.bgw_notify_pid = 0;

	RegisterBackgroundWorker(&worker);
}
