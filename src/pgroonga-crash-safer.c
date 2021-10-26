#include "pgroonga.h"
#include "pgrn-compatible.h"
#include "pgrn-portable.h"

#include "pgrn-crash-safer-statuses.h"
#include "pgrn-file.h"

#include <access/heapam.h>
#ifdef PGRN_SUPPORT_TABLEAM
#	include <access/tableam.h>
#else
#	include <access/htup_details.h>
#endif
#include <access/xact.h>
#include <catalog/pg_database.h>
#ifdef PGRN_HAVE_COMMON_HASHFN_H
#	include <common/hashfn.h>
#endif
#include <executor/spi.h>
#include <miscadmin.h>
#include <pgstat.h>
#include <postmaster/bgworker.h>
#include <storage/ipc.h>
#include <storage/latch.h>
#include <utils/snapmgr.h>
#include <utils/guc.h>
#include <utils/snapmgr.h>
#include <utils/timestamp.h>

#include <groonga.h>


PG_MODULE_MAGIC;

extern PGDLLEXPORT void _PG_init(void);
extern PGDLLEXPORT void
pgroonga_crash_safer_flush_one(Datum datum) pg_attribute_noreturn();
extern PGDLLEXPORT void
pgroonga_crash_safer_detect_one(Datum datum) pg_attribute_noreturn();
extern PGDLLEXPORT void
pgroonga_crash_safer_main(Datum datum) pg_attribute_noreturn();

#define TAG "pgroonga: crash-safer"

static volatile sig_atomic_t PGroongaCrashSaferGotSIGTERM = false;
static volatile sig_atomic_t PGroongaCrashSaferGotSIGHUP = false;
static volatile sig_atomic_t PGroongaCrashSaferGotSIGUSR1 = false;
static int PGroongaCrashSaferFlushNaptime = 60;
static int PGroongaCrashSaferDetectNaptime = 60;
static const char *PGroongaCrashSaferLibraryName = "pgroonga_crash_safer";

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
	int	save_errno = errno;

	PGroongaCrashSaferGotSIGUSR1 = true;
	SetLatch(MyLatch);

	errno = save_errno;
}

void
pgroonga_crash_safer_flush_one(Datum databaseInfoDatum)
{
	uint64 databaseInfo = DatumGetUInt64(databaseInfoDatum);
	Oid databaseOid;
	Oid tableSpaceOid;
	char *databasePath;
	char pgrnDatabasePath[MAXPGPATH];
	grn_ctx ctx;
	grn_obj *db;

	pqsignal(SIGTERM, pgroonga_crash_safer_sigterm);
	pqsignal(SIGHUP, pgroonga_crash_safer_sighup);
	BackgroundWorkerUnblockSignals();

	PGRN_DATABASE_INFO_UNPACK(databaseInfo, databaseOid, tableSpaceOid);

	databasePath = GetDatabasePath(databaseOid, tableSpaceOid);
	join_path_components(pgrnDatabasePath,
						 databasePath,
						 PGrnDatabaseBasename);
	pfree(databasePath);

	ereport(LOG,
			(errmsg(TAG ": flush: %u/%u",
					databaseOid,
					tableSpaceOid)));

	PG_TRY();
	{
		pgstat_report_activity(STATE_RUNNING, TAG ": flushing");

		grn_thread_set_get_limit_func(pgroonga_crash_safer_get_thread_limit,
									  NULL);
		grn_default_logger_set_flags(grn_default_logger_get_flags() |
									 GRN_LOG_PID);

		if (grn_init() != GRN_SUCCESS)
		{
			ereport(ERROR,
					(errcode(ERRCODE_SYSTEM_ERROR),
					 errmsg(TAG ": failed to initialize Groonga")));
		}
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

		if (pgrn_file_exist(pgrnDatabasePath))
		{
			db = grn_db_open(&ctx, pgrnDatabasePath);
		}
		else
		{
			db = grn_db_create(&ctx, pgrnDatabasePath, NULL);
		}
		if (!db)
		{
			ereport(ERROR,
					(errcode(ERRCODE_SYSTEM_ERROR),
					 errmsg(TAG ": failed to open Groonga database: %s",
							ctx.errbuf)));
		}

		while (!PGroongaCrashSaferGotSIGTERM)
		{
			int conditions;

			conditions = WaitLatch(MyLatch,
								   WL_LATCH_SET |
								   WL_TIMEOUT |
								   PGRN_WL_EXIT_ON_PM_DEATH,
								   PGroongaCrashSaferFlushNaptime * 1000L,
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

			if (!pgrn_file_exist(pgrnDatabasePath))
				break;

			grn_obj_flush_recursive(&ctx, db);
		}

		grn_obj_close(&ctx, db);

		grn_ctx_fin(&ctx);

		grn_fin();

		pgstat_report_activity(STATE_IDLE, NULL);
	}
	PG_CATCH();
	{
		pgrn_crash_safer_statuses_stop(NULL, databaseOid, tableSpaceOid);
		PG_RE_THROW();
	}
	PG_END_TRY();

	ereport(LOG,
			(errmsg(TAG ": flush: done: %u/%u",
					databaseOid,
					tableSpaceOid)));

	pgrn_crash_safer_statuses_stop(NULL, databaseOid, tableSpaceOid);

	proc_exit(1);
}

void
pgroonga_crash_safer_detect_one(Datum databaseInfoDatum)
{
	uint64 databaseInfo = DatumGetUInt64(databaseInfoDatum);
	Oid databaseOid;
	Oid tableSpaceOid;

	pqsignal(SIGTERM, pgroonga_crash_safer_sigterm);
	pqsignal(SIGHUP, pgroonga_crash_safer_sighup);
	BackgroundWorkerUnblockSignals();

	PGRN_DATABASE_INFO_UNPACK(databaseInfo, databaseOid, tableSpaceOid);

	PGrnBackgroundWorkerInitializeConnectionByOid(databaseOid, InvalidOid, 0);

	StartTransactionCommand();
	SPI_connect();
	PushActiveSnapshot(GetTransactionSnapshot());
	pgstat_report_activity(STATE_RUNNING, TAG ": checking");

	{
		int result;

		SetCurrentStatementStartTimestamp();
		result = SPI_execute("select extname "
							 "from pg_extension "
							 "where extname = 'pgroonga'",
							 true,
							 0);
		if (result != SPI_OK_SELECT)
		{
			ereport(FATAL,
					(errmsg(TAG ": failed to detect "
							"whether PGroonga is installed or not: %d",
							result)));
		}
		if (SPI_processed == 1) {
			pgrn_crash_safer_statuses_start(NULL,
											databaseOid,
											tableSpaceOid);
			ereport(LOG,
					(errmsg(TAG ": detect: detected: %u/%u",
							databaseOid,
							tableSpaceOid)));
		}
		else
		{
			ereport(LOG,
					(errmsg(TAG ": detect: not detected: %u/%u",
							databaseOid,
							tableSpaceOid)));
		}
	}

	PopActiveSnapshot();
	SPI_finish();
	CommitTransactionCommand();

	pgstat_report_activity(STATE_IDLE, NULL);

	proc_exit(0);
}

static void
pgroonga_crash_safer_detect(HTAB *statuses)
{
	StartTransactionCommand();
	PushActiveSnapshot(GetTransactionSnapshot());
	pgstat_report_activity(STATE_RUNNING,
						   TAG ": detecting all PGroonga enabled databases");

	{
		const LOCKMODE lock = AccessShareLock;
		Relation pg_database;
		PGrnTableScanDesc scan;
		HeapTuple tuple;

		pg_database = pgrn_table_open(DatabaseRelationId, lock);
		scan = pgrn_table_beginscan_catalog(pg_database, 0, NULL);
		for (tuple = heap_getnext(scan, ForwardScanDirection);
			 HeapTupleIsValid(tuple);
			 tuple = heap_getnext(scan, ForwardScanDirection))
		{
			Form_pg_database form = (Form_pg_database) GETSTRUCT(tuple);
			BackgroundWorker worker = {0};
			BackgroundWorkerHandle *handle;
			Oid databaseOid;
			uint64 databaseInfo;

			if (PGroongaCrashSaferGotSIGTERM)
				break;

			if (strcmp(form->datname.data, "template0") == 0)
				continue;
			if (strcmp(form->datname.data, "template1") == 0)
				continue;

#ifdef PGRN_FORM_PG_DATABASE_HAVE_OID
			databaseOid = form->oid;
#else
			databaseOid = HeapTupleGetOid(tuple);
#endif
			databaseInfo =
				PGRN_DATABASE_INFO_PACK(databaseOid, form->dattablespace);

			if (pgrn_crash_safer_statuses_is_processing(statuses,
														databaseOid,
														form->dattablespace))
			{
				continue;
			}

			ereport(LOG,
					(errmsg(TAG ": detect: start: %s(%u/%u)",
							form->datname.data,
							databaseOid,
							form->dattablespace)));
			snprintf(worker.bgw_name,
					 BGW_MAXLEN,
					 TAG ": detect: %s(%u/%u)",
					 form->datname.data,
					 databaseOid,
					 form->dattablespace);
#ifdef PGRN_BACKGROUND_WORKER_HAVE_BGW_TYPE
			snprintf(worker.bgw_type,
					 BGW_MAXLEN,
					 TAG ": detect: %s(%u/%u)",
					 form->datname.data,
					 databaseOid,
					 form->dattablespace);
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
					 "pgroonga_crash_safer_detect_one");
			worker.bgw_main_arg = DatumGetUInt64(databaseInfo);
			worker.bgw_notify_pid = MyProcPid;
			if (!RegisterDynamicBackgroundWorker(&worker, &handle))
				continue;
			WaitForBackgroundWorkerShutdown(handle);

			if (!pgrn_crash_safer_statuses_is_processing(statuses,
														 databaseOid,
														 form->dattablespace))
			{
				ereport(LOG,
						(errmsg(TAG ": detect: not processing: %s(%u/%u)",
								form->datname.data,
								databaseOid,
								form->dattablespace)));
				continue;
			}

			ereport(LOG,
					(errmsg(TAG ": flush: start: %s(%u/%u)",
							form->datname.data,
							databaseOid,
							form->dattablespace)));
			snprintf(worker.bgw_name,
					 BGW_MAXLEN,
					 TAG ": flush: %s(%u/%u)",
					 form->datname.data,
					 databaseOid,
					 form->dattablespace);
#ifdef PGRN_BACKGROUND_WORKER_HAVE_BGW_TYPE
			snprintf(worker.bgw_type,
					 BGW_MAXLEN,
					 TAG ": flush: %s(%u/%u)",
					 form->datname.data,
					 databaseOid,
					 form->dattablespace);
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
			if (!RegisterDynamicBackgroundWorker(&worker, &handle))
			{
				pgrn_crash_safer_statuses_stop(statuses,
											   databaseOid,
											   form->dattablespace);
				continue;
			}
			{
				pid_t pid;
				BgwHandleStatus status;
				status = WaitForBackgroundWorkerStartup(handle, &pid);
				if (status != BGWH_STARTED)
				{
					pgrn_crash_safer_statuses_stop(statuses,
												   databaseOid,
												   form->dattablespace);
				}
			}
		}
		pgrn_table_endscan(scan);
		pgrn_table_close(pg_database, lock);
	}

	PopActiveSnapshot();
	CommitTransactionCommand();
	pgstat_report_activity(STATE_IDLE, NULL);
}

void
pgroonga_crash_safer_main(Datum arg)
{
	HTAB *statuses;
	TimestampTz lastDetectTime = 0;

	pqsignal(SIGTERM, pgroonga_crash_safer_sigterm);
	pqsignal(SIGHUP, pgroonga_crash_safer_sighup);
	pqsignal(SIGUSR1, pgroonga_crash_safer_sigusr1);
	BackgroundWorkerUnblockSignals();

	PGrnBackgroundWorkerInitializeConnection(NULL, NULL, 0);

	statuses = pgrn_crash_safer_statuses_get();
	pgrn_crash_safer_statuses_set_main_pid(statuses, MyProcPid);
	while (!PGroongaCrashSaferGotSIGTERM)
	{
		int conditions;
		TimestampTz now;

		conditions = WaitLatch(MyLatch,
							   WL_LATCH_SET |
							   WL_TIMEOUT |
							   PGRN_WL_EXIT_ON_PM_DEATH,
							   PGroongaCrashSaferDetectNaptime * 1000L,
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

		now = GetCurrentTimestamp();
		if (PGroongaCrashSaferGotSIGUSR1 ||
			TimestampDifferenceExceeds(lastDetectTime,
									   now,
									   PGroongaCrashSaferDetectNaptime * 1000L))
		{
			PGroongaCrashSaferGotSIGUSR1 = false;
			lastDetectTime = now;
			pgroonga_crash_safer_detect(statuses);
		}
	}
	pgrn_crash_safer_statuses_set_main_pid(statuses, 0);

	proc_exit(1);
}

void
_PG_init(void)
{
	BackgroundWorker worker = {0};

	DefineCustomIntVariable("pgroonga_crash_safer.detect_naptime",
							"Duration between each "
							"PGroonga enabled database detection in seconds.",
							"The default is 60 seconds. "
							"It means that PGroonga crash safer tries to "
							"detect all PGroonga enabled databases "
							"per 1 minute.",
							&PGroongaCrashSaferDetectNaptime,
							PGroongaCrashSaferDetectNaptime,
							1,
							INT_MAX,
							PGC_SIGHUP,
							GUC_UNIT_S,
							NULL,
							NULL,
							NULL);

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
