#include "pgroonga.h"
#include "pgrn-compatible.h"
#include "pgrn-portable.h"

#include <sys/types.h>
#include <sys/stat.h>
#ifndef WIN32
#	include <dirent.h>
#	include <unistd.h>
#endif

#include <access/heapam.h>
#ifdef PGRN_SUPPORT_TABLEAM
#	include <access/tableam.h>
#endif
#include <catalog/pg_database.h>
#include <executor/spi.h>
#include <pgstat.h>
#include <postmaster/bgworker.h>
#include <storage/ipc.h>
#include <storage/latch.h>
#include <utils/snapmgr.h>
#include <utils/guc.h>
#include <utils/snapmgr.h>
#include <utils/timestamp.h>

#include <groonga.h>

#define PACK_DATABASE_INFO(oid, tablespace)			\
	((((uint64)(oid)) << (sizeof(Oid) * 8)) + (tablespace))
#define UNPACK_DATABASE_INFO(info, oid, tablespace)						\
	do {																\
		oid = (info) >> (sizeof(Oid) * 8);								\
		tablespace = (info) & ((((uint64)1) << sizeof(Oid) * 8) - 1);	\
	} while (false)

typedef struct pgrn_hash_entry
{
	uint64 key;
	uint64 hash;
	uint32 status;
	pid_t pid;
} pgrn_hash_entry;

#define SH_PREFIX pgrn_hash
#define SH_KEY_TYPE uint64
#define SH_ELEMENT_TYPE pgrn_hash_entry
#define SH_KEY key
#define SH_EQUAL(table, a, b) ((a) == (b))
#define SH_HASH_KEY(table, key) (key)
#define SH_DECLARE
#define SH_DEFINE
#define SH_SCOPE static inline
#include <lib/simplehash.h>


PG_MODULE_MAGIC;

extern PGDLLEXPORT void _PG_init(void);
extern PGDLLEXPORT void
pgroonga_crash_safer_main_one(Datum datum) pg_attribute_noreturn();
extern PGDLLEXPORT void
pgroonga_crash_safer_main(Datum datum) pg_attribute_noreturn();

#define TAG "pgroonga: crash-safer"

static volatile sig_atomic_t PGroongaCrashSaferGotSIGTERM = false;
static volatile sig_atomic_t PGroongaCrashSaferGotSIGHUP = false;
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

void
pgroonga_crash_safer_main_one(Datum databaseInfoDatum)
{
	bool haveGroongaDB = false;
	uint64 databaseInfo = DatumGetUInt64(databaseInfoDatum);
	Oid databaseOid;
	Oid databaseTableSpace;
	char *databasePath;
	char pgrnDatabasePath[MAXPGPATH];

	pqsignal(SIGTERM, pgroonga_crash_safer_sigterm);
	pqsignal(SIGHUP, pgroonga_crash_safer_sighup);
	BackgroundWorkerUnblockSignals();

	UNPACK_DATABASE_INFO(databaseInfo, databaseOid, databaseTableSpace);

	PGrnBackgroundWorkerInitializeConnectionByOid(databaseOid, InvalidOid, 0);

	databasePath = GetDatabasePath(databaseOid, databaseTableSpace);
	join_path_components(pgrnDatabasePath,
						 databasePath,
						 PGrnDatabaseBasename);
	pfree(databasePath);

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
			pgrn_stat_buffer fileStatus;
			haveGroongaDB = (pgrn_stat(pgrnDatabasePath, &fileStatus) == 0);
		}
	}

	PopActiveSnapshot();
	SPI_finish();
	CommitTransactionCommand();

	if (haveGroongaDB)
	{
		grn_ctx ctx;
		grn_obj *db;

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

		db = grn_db_open(&ctx, pgrnDatabasePath);
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

			grn_obj_flush_recursive(&ctx, db);
		}

		grn_obj_close(&ctx, db);

		grn_ctx_fin(&ctx);

		grn_fin();
	}

	pgstat_report_activity(STATE_IDLE, NULL);

	proc_exit(haveGroongaDB ? 1 : 0);
}

static void
pgroonga_crash_safer_detect(pgrn_hash_hash *handles)
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
			databaseInfo = PACK_DATABASE_INFO(databaseOid, form->dattablespace);

			{
				pgrn_hash_entry *entry;
				entry = pgrn_hash_lookup(handles, databaseInfo);
				if (entry)
				{
					if (GetBackgroundWorkerTypeByPid(entry->pid))
					{
						continue;
					}
					else
					{
						pgrn_hash_delete(handles, databaseInfo);
					}
				}
			}

			snprintf(worker.bgw_name,
					 BGW_MAXLEN,
					 TAG ": %s(%u/%u)",
					 form->datname.data,
					 databaseOid,
					 form->dattablespace);
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
					 "%s", PGroongaCrashSaferLibraryName);
			snprintf(worker.bgw_function_name,
					 BGW_MAXLEN,
					 "pgroonga_crash_safer_main_one");
			worker.bgw_main_arg = DatumGetUInt64(databaseInfo);
			worker.bgw_notify_pid = MyProcPid;
			if (!RegisterDynamicBackgroundWorker(&worker, &handle))
				continue;

			{
				bool found;
				BgwHandleStatus status;
				pgrn_hash_entry *entry;
				entry = pgrn_hash_insert(handles, databaseInfo, &found);
				status = WaitForBackgroundWorkerStartup(handle, &(entry->pid));
				if (status != BGWH_STARTED)
				{
					pgrn_hash_delete(handles, databaseInfo);
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
	pgrn_hash_hash *handles;
	TimestampTz lastDetectTime = 0;

	pqsignal(SIGTERM, pgroonga_crash_safer_sigterm);
	pqsignal(SIGHUP, pgroonga_crash_safer_sighup);
	BackgroundWorkerUnblockSignals();

	PGrnBackgroundWorkerInitializeConnection(NULL, NULL, 0);

	handles = pgrn_hash_create(CurrentMemoryContext, 0, NULL);
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
		if (TimestampDifferenceExceeds(lastDetectTime,
									   now,
									   PGroongaCrashSaferDetectNaptime * 1000L))
		{
			lastDetectTime = now;
			pgroonga_crash_safer_detect(handles);
		}
	}
	pgrn_hash_destroy(handles);

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
	worker.bgw_restart_time = BGW_NEVER_RESTART;
	snprintf(worker.bgw_library_name, BGW_MAXLEN,
			 "%s", PGroongaCrashSaferLibraryName);
	snprintf(worker.bgw_function_name, BGW_MAXLEN,
			 "pgroonga_crash_safer_main");
	worker.bgw_main_arg = 0;
	worker.bgw_notify_pid = 0;

	RegisterBackgroundWorker(&worker);
}
