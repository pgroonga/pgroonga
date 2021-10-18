#include "pgrn-compatible.h"

#include <access/heapam.h>
#include <access/relscan.h>
#ifdef PGRN_SUPPORT_TABLEAM
#	include <access/tableam.h>
#endif
#include <access/xact.h>
#include <catalog/pg_database.h>
#ifndef PGRN_FORM_PG_DATABASE_HAVE_OID
#	include <commands/dbcommands.h>
#endif
#include <executor/spi.h>
#include <fmgr.h>
#include <miscadmin.h>
#include <pgstat.h>
#include <postmaster/bgworker.h>
#include <storage/ipc.h>
#include <storage/latch.h>
#include <utils/guc.h>
#include <utils/snapmgr.h>

PG_MODULE_MAGIC;

extern PGDLLEXPORT void _PG_init(void);
extern PGDLLEXPORT void
pgroonga_wal_applier_apply(Datum datum) pg_attribute_noreturn();
extern PGDLLEXPORT void
pgroonga_wal_applier_main(Datum datum) pg_attribute_noreturn();

#define TAG "pgroonga: wal-applier"

static volatile sig_atomic_t PGroongaWALApplierGotSIGTERM = false;
static volatile sig_atomic_t PGroongaWALApplierGotSIGHUP = false;
static int PGroongaWALApplierNaptime = 60;
static const char *PGroongaWALApplierLibraryName = "pgroonga_wal_applier";

static void
pgroonga_wal_applier_sigterm(SIGNAL_ARGS)
{
	int	save_errno = errno;

	PGroongaWALApplierGotSIGTERM = true;
	SetLatch(MyLatch);

	errno = save_errno;
}

static void
pgroonga_wal_applier_sighup(SIGNAL_ARGS)
{
	int	save_errno = errno;

	PGroongaWALApplierGotSIGHUP = true;
	SetLatch(MyLatch);

	errno = save_errno;
}

void
pgroonga_wal_applier_apply(Datum databaseOidDatum)
{
	Oid databaseOid = DatumGetObjectId(databaseOidDatum);
	PGrnBackgroundWorkerInitializeConnectionByOid(databaseOid, InvalidOid, 0);

	StartTransactionCommand();
	SPI_connect();
	PushActiveSnapshot(GetTransactionSnapshot());
	pgstat_report_activity(STATE_RUNNING, TAG ": applying");

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
		if (SPI_processed == 1)
		{
			SetCurrentStatementStartTimestamp();
			result = SPI_execute("select pgroonga_wal_apply()", true, 0);
			if (result != SPI_OK_SELECT)
			{
				ereport(FATAL,
						(errmsg(TAG ": failed to apply WAL: %d",
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
pgroonga_wal_applier_apply_all(void)
{
	StartTransactionCommand();
	PushActiveSnapshot(GetTransactionSnapshot());
	pgstat_report_activity(STATE_RUNNING, TAG ": applying all databases");

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

			if (strcmp(form->datname.data, "template0") == 0)
				continue;
			if (strcmp(form->datname.data, "template1") == 0)
				continue;

#ifdef PGRN_FORM_PG_DATABASE_HAVE_OID
			databaseOid = form->oid;
#else
			databaseOid = get_database_oid(form->datname.data, true);
#endif
			snprintf(worker.bgw_name,
					 BGW_MAXLEN,
					 TAG ": %s(%u)",
					 form->datname.data,
					 databaseOid);
#ifdef PGRN_BACKGROUND_WORKER_HAVE_BGW_TYPE
			snprintf(worker.bgw_type, BGW_MAXLEN, TAG);
#endif
			worker.bgw_flags =
				BGWORKER_SHMEM_ACCESS |
				BGWORKER_BACKEND_DATABASE_CONNECTION;
			worker.bgw_start_time = BgWorkerStart_ConsistentState;
			worker.bgw_restart_time = BGW_NEVER_RESTART;
			snprintf(worker.bgw_library_name,
					 BGW_MAXLEN,
					 "%s", PGroongaWALApplierLibraryName);
			snprintf(worker.bgw_function_name,
					 BGW_MAXLEN,
					 "pgroonga_wal_applier_apply");
			worker.bgw_main_arg = DatumGetObjectId(databaseOid);
			worker.bgw_notify_pid = MyProcPid;
			if (!RegisterDynamicBackgroundWorker(&worker, &handle))
				continue;
			WaitForBackgroundWorkerShutdown(handle);
		}
		pgrn_table_endscan(scan);
		pgrn_table_close(pg_database, lock);
	}

	PopActiveSnapshot();
	CommitTransactionCommand();
	pgstat_report_activity(STATE_IDLE, NULL);
}

void
pgroonga_wal_applier_main(Datum arg)
{
	pqsignal(SIGTERM, pgroonga_wal_applier_sigterm);
	pqsignal(SIGHUP, pgroonga_wal_applier_sighup);
	BackgroundWorkerUnblockSignals();

	PGrnBackgroundWorkerInitializeConnection(NULL, NULL, 0);

	while (!PGroongaWALApplierGotSIGTERM)
	{
		WaitLatch(MyLatch,
				  WL_LATCH_SET |
				  WL_TIMEOUT |
				  PGRN_WL_EXIT_ON_PM_DEATH,
				  PGroongaWALApplierNaptime * 1000L,
				  PG_WAIT_EXTENSION);
		ResetLatch(MyLatch);

		CHECK_FOR_INTERRUPTS();

		if (PGroongaWALApplierGotSIGHUP)
		{
			PGroongaWALApplierGotSIGHUP = false;
			ProcessConfigFile(PGC_SIGHUP);
		}

		pgroonga_wal_applier_apply_all();
	}

	proc_exit(1);
}

void
_PG_init(void)
{
	BackgroundWorker worker = {0};

	DefineCustomIntVariable("pgroonga_wal_applier.naptime",
							"Duration between each check in seconds.",
							"The default is 60 seconds. "
							"It means that PGroonga WAL applier tries to "
							"apply all pending PGroonga WAL "
							"in all PGroonga available databases "
							"per 1 minute.",
							&PGroongaWALApplierNaptime,
							PGroongaWALApplierNaptime,
							1,
							INT_MAX,
							PGC_SIGHUP,
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
	worker.bgw_restart_time = BGW_NEVER_RESTART;
	snprintf(worker.bgw_library_name, BGW_MAXLEN,
			 "%s", PGroongaWALApplierLibraryName);
	snprintf(worker.bgw_function_name, BGW_MAXLEN,
			 "pgroonga_wal_applier_main");
	worker.bgw_main_arg = 0;
	worker.bgw_notify_pid = 0;

	RegisterBackgroundWorker(&worker);
}
