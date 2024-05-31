#include "pgrn-compatible.h"

#include <fmgr.h>
#include <miscadmin.h>
#include <postmaster/bgworker.h>
#include <storage/ipc.h>

PG_MODULE_MAGIC;

extern PGDLLEXPORT void _PG_init(void);
extern PGDLLEXPORT void pgroonga_primary_maintainer_main(Datum datum)
	pg_attribute_noreturn();

#define TAG "pgroonga: primary-maintainer"

static const char *PGroongaPrimaryMaintainerLibraryName =
	"pgroonga_primary_maintainer";

void
pgroonga_primary_maintainer_main(Datum arg)
{
	elog(LOG, TAG ": debug");
	proc_exit(1);
}

void
_PG_init(void)
{
	BackgroundWorker worker = {0};

	if (!process_shared_preload_libraries_in_progress)
		return;

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
