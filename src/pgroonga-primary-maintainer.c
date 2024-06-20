#include "pgrn-compatible.h"

#include <fmgr.h>

PG_MODULE_MAGIC;

#define TAG "pgroonga: primary-maintainer"

extern PGDLLEXPORT void _PG_init(void);

void
_PG_init(void)
{
	elog(LOG, TAG ": debug");
}
