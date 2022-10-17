#include <postgres.h>

#include <access/heapam.h>
#include <fmgr.h>
#include <miscadmin.h>
#include <postmaster/bgworker.h>


PG_MODULE_MAGIC;

void _PG_init(void);
void pgroonga_standby_maintainer_aply_all(void);
void pgroonga_standby_maintainer_vacuum_all(void);
void pgroonga_standby_maintainer_main(Datum arg);

#define TAG "pgroonga: standby-maintainer"

static int PGroongaStandbyMaintainerNaptime = 60;
static const char *PGroongaStandbyMaintainerLibraryName = "pgroonga_standby_maintainer";

void
pgroonga_standby_maintainer_aply_all(void)
{

}

void
pgroonga_standby_maintainer_vacuum_all(void)
{

}

void
pgroonga_standby_maintainer_main(Datum arg)
{

}

void
_PG_init(void)
{
	BackgroundWorker worker = {0};

	DefineCustomIntVariable("pgroonga_standby_maintainer.naptime",                // parameter name
							"Duration between each check in seconds.",            // short discription
							"The default is 60 seconds. "                         // long discription
							"It means that PGroonga standby maintainer tries to "
							"apply all pending PGroonga WAL and remove internal "
							"unused Groonga tables, columns and records in all "
							"PGroonga available databases per 1 minute.",
							&PGroongaStandbyMaintainerNaptime,                    // name of variable to store value
							PGroongaStandbyMaintainerNaptime,                     // default value
							1,                                                    // minimal value
							INT_MAX,                                              // maximun value
							PGC_SIGHUP,                                           // timing of changing value
							0,                                                    // flags
							NULL,                                                 // process of checking (hook)
							NULL,                                                 // process of assignment (hook)
							NULL);                                                // process of showing (hook)

	if (!process_shared_preload_libraries_in_progress)
		return;

	snprintf(worker.bgw_name, BGW_MAXLEN, TAG ": main");
	snprintf(worker.bgw_type, BGW_MAXLEN, TAG);
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
