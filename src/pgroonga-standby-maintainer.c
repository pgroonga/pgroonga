#include "pgrn-compatible.h"
#include "pgrn-database-info.h"

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
pgroonga_standby_maintainer_apply_wal(Datum datum) pg_attribute_noreturn();
extern PGDLLEXPORT void
pgroonga_standby_maintainer_maintain(Datum datum) pg_attribute_noreturn();
extern PGDLLEXPORT void
pgroonga_standby_maintainer_main(Datum datum) pg_attribute_noreturn();

#define TAG "pgroonga: standby-maintainer"

static volatile sig_atomic_t PGroongaStandbyMaintainerGotSIGTERM = false;
static volatile sig_atomic_t PGroongaStandbyMaintainerGotSIGHUP = false;
static int PGroongaStandbyMaintainerNaptime = 60;
static int PGroongaStandbyMaintainerMaxParallelWALAppliersPerDB = 0;
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

/*
 * char bgw_extra[BGW_EXTRALEN]
 *
 * * The first 4 or 8 bytes are used for index OID
 * * The rest are used for index name
 *
 * |<----index OID----->|<-----------index name------------>|
 * |sizeof(unsigned int)|BGW_EXTRALEN - sizeof(unsigned int)|
 */
#define BGWORKER_GET_INDEX_OID(worker) (*((Oid *)((worker)->bgw_extra)))
#define BGWORKER_SET_INDEX_OID(worker, oid)		\
	*((Oid *)((worker)->bgw_extra)) = (oid)
#define BGWORKER_GET_INDEX_NAME(worker)				\
	((char *)((Oid *)((worker)->bgw_extra) + 1))
#define BGWORKER_SET_INDEX_NAME(worker, name, nameSize)	do {			\
		size_t maxSize = BGW_EXTRALEN - sizeof(Oid) - 1;				\
		size_t realSize = (nameSize) > maxSize ? maxSize : (nameSize);	\
		if (realSize > 0)												\
		{																\
			memcpy((char *)((Oid *)((worker)->bgw_extra) + 1),			\
				   (name),												\
				   realSize);											\
		}																\
		(worker)->bgw_extra[sizeof(Oid) + realSize] = '\0';				\
	} while (false)

static void
pgroonga_standby_maintainer_execute_apply_wal(Oid databaseOid,
											  Oid tableSpaceOid,
											  Oid indexOid,
											  const char *indexName)
{
	SPIPlanPtr plan;
	int nArgs = 1;
	Oid argTypes[1] = {OIDOID};
	Datum args[1] = {ObjectIdGetDatum(indexOid)};
	char nulls[1] = {' '};
	int result;

	SetCurrentStatementStartTimestamp();
	plan = SPI_prepare("SELECT pgroonga_wal_apply($1::regclass::text::cstring)",
					   nArgs,
					   argTypes);
	result = SPI_execute_plan(plan, args, nulls, false, 0);
	if (result != SPI_OK_SELECT)
	{
		ereport(FATAL,
				(errmsg(TAG ": failed to apply WAL: "
						"%s(%u/%u/%u): %d",
						indexName,
						databaseOid,
						tableSpaceOid,
						indexOid,
						result)));
	}
}

void
pgroonga_standby_maintainer_apply_wal(Datum databaseInfoDatum)
{
	uint64 databaseInfo = DatumGetUInt64(databaseInfoDatum);
	Oid databaseOid;
	Oid tableSpaceOid;

	BackgroundWorkerUnblockSignals();

	PGRN_DATABASE_INFO_UNPACK(databaseInfo, databaseOid, tableSpaceOid);

	BackgroundWorkerInitializeConnectionByOid(databaseOid, InvalidOid, 0);

	StartTransactionCommand();
	SPI_connect();
	PushActiveSnapshot(GetTransactionSnapshot());
	pgstat_report_activity(STATE_RUNNING, TAG ": applying WAL");

	{
		Oid indexOid = BGWORKER_GET_INDEX_OID(MyBgworkerEntry);
		const char *indexName = BGWORKER_GET_INDEX_NAME(MyBgworkerEntry);
		pgroonga_standby_maintainer_execute_apply_wal(databaseOid,
													  tableSpaceOid,
													  indexOid,
													  indexName);
	}

	PopActiveSnapshot();
	SPI_finish();
	CommitTransactionCommand();
	pgstat_report_activity(STATE_IDLE, NULL);

	proc_exit(0);
}

void
pgroonga_standby_maintainer_maintain(Datum databaseInfoDatum)
{
	uint64 databaseInfo = DatumGetUInt64(databaseInfoDatum);
	Oid databaseOid;
	Oid tableSpaceOid;

	BackgroundWorkerUnblockSignals();

	PGRN_DATABASE_INFO_UNPACK(databaseInfo, databaseOid, tableSpaceOid);

	BackgroundWorkerInitializeConnectionByOid(databaseOid, InvalidOid, 0);

	StartTransactionCommand();
	SPI_connect();
	PushActiveSnapshot(GetTransactionSnapshot());
	pgstat_report_activity(STATE_RUNNING, TAG ": maintaining");

	{
		int result;
		int nRunningWorkers = 0;
		bool error = false;
		uint64 i;
		uint64 nIndexes;
		int nMaxWorkers = PGroongaStandbyMaintainerMaxParallelWALAppliersPerDB;
		BackgroundWorker *workers;
		BackgroundWorkerHandle **workerHandles;

		SetCurrentStatementStartTimestamp();
		result = SPI_execute("SELECT class.oid AS index_oid, "
							 "       (namespace.nspname || "
							 "        '.' || "
							 "        class.relname) AS index_name "
							 "  FROM pg_catalog.pg_class AS class "
							 "  JOIN pg_catalog.pg_namespace AS namespace "
							 "    ON class.relnamespace = namespace.oid "
							 " WHERE class.relam = ("
							 "   SELECT oid "
							 "     FROM pg_catalog.pg_am "
							 "    WHERE amname = 'pgroonga'"
							 " )",
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

		nIndexes = SPI_processed;
		workers = palloc(sizeof(BackgroundWorker) * nIndexes);
		workerHandles = palloc(sizeof(BackgroundWorkerHandle *) * nIndexes);
		for (i = 0; i < nIndexes; i++)
		{
			BackgroundWorker *worker = &(workers[i]);
			bool isNull;
			Datum indexOidDatum;
			Datum indexNameDatum;
			const char *indexName;
			size_t indexNameSize;

			memset(&(workers[i]), 0, sizeof(BackgroundWorker));
			workerHandles[i] = NULL;

			indexOidDatum = SPI_getbinval(SPI_tuptable->vals[i],
										  SPI_tuptable->tupdesc,
										  1,
										  &isNull);
			if (isNull)
			{
				BGWORKER_SET_INDEX_OID(worker, InvalidOid);
			}
			else
			{
				BGWORKER_SET_INDEX_OID(worker, DatumGetObjectId(indexOidDatum));
			}

			indexNameDatum = SPI_getbinval(SPI_tuptable->vals[i],
										   SPI_tuptable->tupdesc,
										   2,
										   &isNull);
			if (isNull)
			{
				indexName = NULL;
				indexNameSize = 0;
			}
			else
			{
				indexName = VARDATA_ANY(indexNameDatum);
				indexNameSize = VARSIZE_ANY_EXHDR(indexNameDatum);
			}
			BGWORKER_SET_INDEX_NAME(worker, indexName, indexNameSize);
		}

		for (i = 0; i < nIndexes; i++)
		{
			BackgroundWorker *worker = &(workers[i]);
			Oid indexOid = BGWORKER_GET_INDEX_OID(worker);
			const char *indexName = BGWORKER_GET_INDEX_NAME(worker);
			if (!OidIsValid(indexOid))
				continue;
			if (!indexName)
				continue;

			if (nMaxWorkers == 0)
			{
				pgroonga_standby_maintainer_execute_apply_wal(databaseOid,
															  tableSpaceOid,
															  indexOid,
															  indexName);
				continue;
			}

			snprintf(worker->bgw_name,
					 BGW_MAXLEN,
					 TAG ": apply WAL: %s(%u/%u/%u)",
					 indexName,
					 databaseOid,
					 tableSpaceOid,
					 indexOid);
			snprintf(worker->bgw_type, BGW_MAXLEN, "%s", worker->bgw_name);
			worker->bgw_flags =
				BGWORKER_SHMEM_ACCESS |
				BGWORKER_BACKEND_DATABASE_CONNECTION;
			worker->bgw_start_time = BgWorkerStart_ConsistentState;
			worker->bgw_restart_time = BGW_NEVER_RESTART;
			snprintf(worker->bgw_library_name,
					 BGW_MAXLEN,
					 "%s", PGroongaStandbyMaintainerLibraryName);
			snprintf(worker->bgw_function_name,
					 BGW_MAXLEN,
					 "pgroonga_standby_maintainer_apply_wal");
			worker->bgw_main_arg = databaseInfoDatum;
			worker->bgw_notify_pid = MyProcPid;

			while (nRunningWorkers >= nMaxWorkers)
			{
				int events;
				uint64_t j;

				for (j = 0; j < i; j++)
				{
					BackgroundWorkerHandle *handle = workerHandles[j];
					pid_t pid;
					BgwHandleStatus status;

					if (!handle)
						continue;

					status = GetBackgroundWorkerPid(handle, &pid);
					if (status == BGWH_STOPPED)
					{
						workerHandles[j] = NULL;
						nRunningWorkers--;
					}
				}

				events = WaitLatch(MyLatch,
								   WL_LATCH_SET |
								   WL_TIMEOUT |
								   PGRN_WL_EXIT_ON_PM_DEATH,
								   60 * 1000,
								   WAIT_EVENT_BGWORKER_SHUTDOWN);
				if (events & PGRN_WL_EXIT_ON_PM_DEATH)
				{
					error = true;
					break;
				}
			}
			if (error)
				break;

			if (!RegisterDynamicBackgroundWorker(worker, &(workerHandles[i])))
				continue;

			{
				BackgroundWorkerHandle *handle = workerHandles[i];
				pid_t pid;
				BgwHandleStatus status;
				status = WaitForBackgroundWorkerStartup(handle, &pid);
				if (status == BGWH_STARTED)
				{
					nRunningWorkers++;
				}
				else
				{
					workerHandles[i] = NULL;
				}
			}
		}
		if (!error)
		{
			for (i = 0; i < nIndexes; i++)
			{
				BackgroundWorkerHandle *handle = workerHandles[i];
				BgwHandleStatus status;

				if (!handle)
					continue;

				status = WaitForBackgroundWorkerShutdown(handle);
				if (status == BGWH_POSTMASTER_DIED)
				{
					error = true;
					break;
				}
				nRunningWorkers--;
			}
		}
		pfree(workers);
		pfree(workerHandles);

		if (!error && nIndexes > 0)
		{
			SetCurrentStatementStartTimestamp();
			result = SPI_execute("SELECT pgroonga_vacuum()", true, 0);
			if (result != SPI_OK_SELECT)
			{
				ereport(FATAL,
						(errmsg(TAG ": failed to vacuum: %d/%d: %d",
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
			Oid tableSpaceOid;

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
			tableSpaceOid = form->dattablespace;

			snprintf(worker.bgw_name,
					 BGW_MAXLEN,
					 TAG ": maintain: %s(%u/%u)",
					 form->datname.data,
					 databaseOid,
					 tableSpaceOid);
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
			worker.bgw_main_arg =
				PGRN_DATABASE_INFO_PACK(databaseOid, tableSpaceOid);
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
	DefineCustomIntVariable("pgroonga_standby_maintainer.max_parallel_wal_appliers_per_db",
							"The max number of parallel WAL applier processes "
							"per DB.",
							"The default is 0. "
							"It means that no parallel WAL applier process "
							"is used.",
							&PGroongaStandbyMaintainerMaxParallelWALAppliersPerDB,
							PGroongaStandbyMaintainerMaxParallelWALAppliersPerDB,
							0,
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
