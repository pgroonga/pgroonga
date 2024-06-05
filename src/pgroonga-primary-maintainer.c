#include "pgrn-compatible.h"

#include <access/relscan.h>
#include <fmgr.h>
#include <miscadmin.h>
#include <postmaster/bgworker.h>
#include <storage/ipc.h>
#include <utils/guc.h>

PG_MODULE_MAGIC;

extern PGDLLEXPORT void _PG_init(void);
extern PGDLLEXPORT void pgroonga_primary_maintainer_main(Datum datum)
	pg_attribute_noreturn();

#define TAG "pgroonga: primary-maintainer"

static int PGroongaPrimaryMaintainerReindexThreshold =
	(1024 * 1024 * 1024) / BLCKSZ; // 1GB in size
static const char *PGroongaPrimaryMaintainerLibraryName =
	"pgroonga_primary_maintainer";

void
pgroonga_primary_maintainer_main(Datum arg)
{
	elog(LOG,
		 TAG ": reindex_threshold=%d",
		 PGroongaPrimaryMaintainerReindexThreshold);
	proc_exit(1);
}

void
_PG_init(void)
{
	BackgroundWorker worker = {0};

	if (!process_shared_preload_libraries_in_progress)
		return;

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
