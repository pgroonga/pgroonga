#include <postgres.h>

#include <fmgr.h>
#include <miscadmin.h>
#include <postmaster/bgworker.h>

void _PG_init(void);
void pgroonga_standby_maintainer_vacuum_all(void);
void pgroonga_standby_maintainer_main(Datum arg);

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

	if (!process_shared_preload_libraries_in_progress)
		return;

	RegisterBackgroundWorker(&worker);
}
