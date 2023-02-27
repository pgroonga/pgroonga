#include "pgrn-compatible.h"

#include <access/heapam.h>
#include <access/relscan.h>
#include <access/xact.h>
#include <catalog/pg_database.h>
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
pgroonga_standby_maintainer_maintain(Datum datum) pg_attribute_noreturn();
extern PGDLLEXPORT void
pgroonga_standby_maintainer_main(Datum datum) pg_attribute_noreturn();

#define TAG "pgroonga: standby-maintainer"

static volatile sig_atomic_t PGroongaStandbyMaintainerGotSIGTERM = false;
static volatile sig_atomic_t PGroongaStandbyMaintainerGotSIGHUP = false;
static int PGroongaStandbyMaintainerNaptime = 60;
static const char *PGroongaStandbyMaintainerLibraryName = "pgroonga_standby_maintainer";

static void
pgroonga_standby_maintainer_sigterm(SIGNAL_ARGS)
{
	int save_errno = errno;

	PGroongaStandbyMaintainerGotSIGTERM = true;
	SetLatch(MyLatch);

	errno = save_errno;
}

static void
pgroonga_standby_maintainer_sighup(SIGNAL_ARGS)
{
	int save_errno = errno;

	PGroongaStandbyMaintainerGotSIGHUP = true;
	SetLatch(MyLatch);

	errno = save_errno;
}

void
pgroonga_standby_maintainer_maintain(Datum databaseOidDatum)
{
	Oid databaseOid = DatumGetObjectId(databaseOidDatum);
	BackgroundWorkerInitializeConnectionByOid(databaseOid, InvalidOid, 0);

	StartTransactionCommand();
	SPI_connect();
	PushActiveSnapshot(GetTransactionSnapshot());
	pgstat_report_activity(STATE_RUNNING, TAG ": maintaining");

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
			result = SPI_execute("select pgroonga_vacuum()", true, 0);
			if (result != SPI_OK_SELECT)
			{
				ereport(FATAL,
						(errmsg(TAG ": failed to vacuum: %d",
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
pgroonga_standby_maintainer_maintain_all(void)
{
	StartTransactionCommand();
	PushActiveSnapshot(GetTransactionSnapshot());
	pgstat_report_activity(STATE_RUNNING, TAG ": maintaining all databases");

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

			if (PGroongaStandbyMaintainerGotSIGTERM)
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
			snprintf(worker.bgw_name,
					 BGW_MAXLEN,
					 TAG ": maintain: %s(%u)",
					 form->datname.data,
					 databaseOid);
			snprintf(worker.bgw_type, BGW_MAXLEN, "%s", worker.bgw_name);
			worker.bgw_flags =
				BGWORKER_SHMEM_ACCESS |
				BGWORKER_BACKEND_DATABASE_CONNECTION;
			worker.bgw_start_time = BgWorkerStart_ConsistentState;
			worker.bgw_restart_time = BGW_NEVER_RESTART;
			snprintf(worker.bgw_library_name,
					 BGW_MAXLEN,
					 "%s", PGroongaStandbyMaintainerLibraryName);
			snprintf(worker.bgw_function_name,
					 BGW_MAXLEN,
					 "pgroonga_standby_maintainer_maintain");
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
pgroonga_standby_maintainer_main(Datum arg)
{
	pqsignal(SIGTERM, pgroonga_standby_maintainer_sigterm);
	pqsignal(SIGHUP, pgroonga_standby_maintainer_sighup);
	BackgroundWorkerUnblockSignals();

	BackgroundWorkerInitializeConnection(NULL, NULL, 0);

	while (!PGroongaStandbyMaintainerGotSIGTERM)
	{
		WaitLatch(MyLatch,
				  WL_LATCH_SET |
				  WL_TIMEOUT |
				  PGRN_WL_EXIT_ON_PM_DEATH,
				  PGroongaStandbyMaintainerNaptime * 1000L,
				  PG_WAIT_EXTENSION);
		ResetLatch(MyLatch);

		CHECK_FOR_INTERRUPTS();

		if (PGroongaStandbyMaintainerGotSIGHUP)
		{
			PGroongaStandbyMaintainerGotSIGHUP = false;
			ProcessConfigFile(PGC_SIGHUP);
		}

		pgroonga_standby_maintainer_maintain_all();
	}

	proc_exit(1);
}

void
_PG_init(void)
{
	BackgroundWorker worker = {0};

	DefineCustomIntVariable("pgroonga_standby_maintainer.naptime",
							"Duration between each check in seconds.",
							"The default is 60 seconds. "
							"It means that PGroonga standby maintainer tries to "
							"apply all pending PGroonga WAL and remove internal "
							"unused Groonga tables, columns and records in all "
							"PGroonga available databases per 1 minute.",
							&PGroongaStandbyMaintainerNaptime,
							PGroongaStandbyMaintainerNaptime,
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
	snprintf(worker.bgw_type, BGW_MAXLEN, "%s", worker.bgw_name);
	worker.bgw_flags =
		BGWORKER_SHMEM_ACCESS |
		BGWORKER_BACKEND_DATABASE_CONNECTION;
	worker.bgw_start_time = BgWorkerStart_ConsistentState;
	worker.bgw_restart_time = BGW_NEVER_RESTART;
	snprintf(worker.bgw_library_name, BGW_MAXLEN,
			 "%s", PGroongaStandbyMaintainerLibraryName);
	snprintf(worker.bgw_function_name, BGW_MAXLEN,
			 "pgroonga_standby_maintainer_main");
	worker.bgw_main_arg = 0;
	worker.bgw_notify_pid = 0;

	RegisterBackgroundWorker(&worker);
}
