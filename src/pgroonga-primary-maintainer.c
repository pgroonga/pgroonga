#include "pgrn-compatible.h"
#include "pgrn-database-info.h"

#include <access/heapam.h>
#include <access/relscan.h>
#include <catalog/pg_database.h>
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
extern PGDLLEXPORT void pgroonga_primary_maintainer_wal_size_check(Datum datum)
	pg_attribute_noreturn();
extern PGDLLEXPORT void pgroonga_primary_maintainer_main(Datum datum)
	pg_attribute_noreturn();

#define TAG "pgroonga: primary-maintainer"

static volatile sig_atomic_t PGroongaPrimaryMaintainerGotSIGTERM = false;
static volatile sig_atomic_t PGroongaPrimaryMaintainerGotSIGHUP = false;
static int PGroongaPrimaryMaintainerNaptime = 60;
static int PGroongaPrimaryMaintainerReindexThreshold =
	(1024 * 1024 * 1024) / BLCKSZ; // 1GB in size
static const char *PGroongaPrimaryMaintainerLibraryName =
	"pgroonga_primary_maintainer";

static void
pgroonga_primary_maintainer_sigterm(SIGNAL_ARGS)
{
	int save_errno = errno;

	PGroongaPrimaryMaintainerGotSIGTERM = true;
	SetLatch(MyLatch);

	errno = save_errno;
}

static void
pgroonga_primary_maintainer_sighup(SIGNAL_ARGS)
{
	int save_errno = errno;

	PGroongaPrimaryMaintainerGotSIGHUP = true;
	SetLatch(MyLatch);

	errno = save_errno;
}

void
pgroonga_primary_maintainer_wal_size_check(Datum databaseInfoDatum)
{
	// todo
	// * Check WAL size
	// * Run reindex if threshold is over
	elog(LOG, TAG ": DEBUG pgroonga_primary_maintainer_wal_size_check()");
	proc_exit(0);
}

static void
pgroonga_primary_maintainer_wal_size_check_all(void)
{
	StartTransactionCommand();
	PushActiveSnapshot(GetTransactionSnapshot());
	pgstat_report_activity(STATE_RUNNING, TAG ": checking all databases");

	{
		const LOCKMODE lock = AccessShareLock;
		Relation pg_database;
		TableScanDesc scan;
		HeapTuple tuple;

		pg_database = table_open(DatabaseRelationId, lock);
		scan = table_beginscan_catalog(pg_database, 0, NULL);
		for (tuple = heap_getnext(scan, ForwardScanDirection);
			 HeapTupleIsValid(tuple);
			 tuple = heap_getnext(scan, ForwardScanDirection))
		{
			Form_pg_database form = (Form_pg_database) GETSTRUCT(tuple);
			BackgroundWorker worker = {0};
			BackgroundWorkerHandle *handle;
			Oid databaseOid;
			Oid tableSpaceOid;

			if (PGroongaPrimaryMaintainerGotSIGTERM)
				break;

			if (strcmp(form->datname.data, "template0") == 0)
				continue;
			if (strcmp(form->datname.data, "template1") == 0)
				continue;

			databaseOid = form->oid;
			tableSpaceOid = form->dattablespace;

			snprintf(worker.bgw_name,
					 BGW_MAXLEN,
					 TAG ": wal size check: %s(%u/%u)",
					 form->datname.data,
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
					 PGroongaPrimaryMaintainerLibraryName);
			snprintf(worker.bgw_function_name,
					 BGW_MAXLEN,
					 "pgroonga_primary_maintainer_wal_size_check");
			worker.bgw_main_arg =
				PGRN_DATABASE_INFO_PACK(databaseOid, tableSpaceOid);
			worker.bgw_notify_pid = MyProcPid;
			if (!RegisterDynamicBackgroundWorker(&worker, &handle))
				continue;
			WaitForBackgroundWorkerShutdown(handle);
		}
		table_endscan(scan);
		table_close(pg_database, lock);
	}

	PopActiveSnapshot();
	CommitTransactionCommand();
	pgstat_report_activity(STATE_IDLE, NULL);
}

void
pgroonga_primary_maintainer_main(Datum arg)
{
	pqsignal(SIGTERM, pgroonga_primary_maintainer_sigterm);
	pqsignal(SIGHUP, pgroonga_primary_maintainer_sighup);
	BackgroundWorkerUnblockSignals();

	elog(LOG, TAG ": naptime=%d", PGroongaPrimaryMaintainerNaptime);
	elog(LOG,
		 TAG ": reindex_threshold=%d",
		 PGroongaPrimaryMaintainerReindexThreshold);

	BackgroundWorkerInitializeConnection(NULL, NULL, 0);

	while (!PGroongaPrimaryMaintainerGotSIGTERM)
	{
		WaitLatch(MyLatch,
				  WL_LATCH_SET | WL_TIMEOUT | PGRN_WL_EXIT_ON_PM_DEATH,
				  PGroongaPrimaryMaintainerNaptime * 1000L,
				  PG_WAIT_EXTENSION);
		ResetLatch(MyLatch);

		CHECK_FOR_INTERRUPTS();

		if (PGroongaPrimaryMaintainerGotSIGHUP)
		{
			PGroongaPrimaryMaintainerGotSIGHUP = false;
			ProcessConfigFile(PGC_SIGHUP);
		}

		pgroonga_primary_maintainer_wal_size_check_all();
	}
	proc_exit(1);
}

void
_PG_init(void)
{
	BackgroundWorker worker = {0};

	if (!process_shared_preload_libraries_in_progress)
		return;

	DefineCustomIntVariable(
		"pgroonga_primary_maintainer.naptime",
		"Duration between each check in seconds.",
		"The default is 60 seconds. "
		"It means that PGroonga primary maintainer checks "
		"the number of blocks in WAL every minute and runs "
		"REINDEX CONCURRENTLY` if the threshold is exceeded.",
		&PGroongaPrimaryMaintainerNaptime,
		PGroongaPrimaryMaintainerNaptime,
		1,
		INT_MAX,
		PGC_SIGHUP,
		GUC_UNIT_S,
		NULL,
		NULL,
		NULL);

	DefineCustomIntVariable(
		"pgroonga_primary_maintainer.reindex_threshold",
		"Specifies the number of WAL blocks. "
		"If the specified value is exceeded, `REINDEX CONCURRENTLY` is run.",
		"The default is 1GB in size. "
		"This parameter specifies the number of blocks, but you can also be "
		"specified by size. "
		"When specifying by size, you must always add a unit. "
		"You can use units `B`, `kB`, `MB`, `GB`, and `TB`.",
		&PGroongaPrimaryMaintainerReindexThreshold,
		PGroongaPrimaryMaintainerReindexThreshold,
		1,
		INT_MAX,
		PGC_SIGHUP,
		GUC_UNIT_BLOCKS,
		NULL,
		NULL,
		NULL);

	snprintf(worker.bgw_name, BGW_MAXLEN, TAG ": main");
	snprintf(worker.bgw_type, BGW_MAXLEN, "%s", worker.bgw_name);
	worker.bgw_flags =
		BGWORKER_SHMEM_ACCESS | BGWORKER_BACKEND_DATABASE_CONNECTION;
	worker.bgw_start_time = BgWorkerStart_RecoveryFinished;
	worker.bgw_restart_time = BGW_NEVER_RESTART;
	snprintf(worker.bgw_library_name,
			 BGW_MAXLEN,
			 "%s",
			 PGroongaPrimaryMaintainerLibraryName);
	snprintf(worker.bgw_function_name,
			 BGW_MAXLEN,
			 "pgroonga_primary_maintainer_main");
	worker.bgw_main_arg = 0;
	worker.bgw_notify_pid = 0;

	RegisterBackgroundWorker(&worker);
}
