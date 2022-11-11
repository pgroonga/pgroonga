#pragma once

#include "pgrn-compatible.h"

extern PGDLLEXPORT void
pgroonga_standby_maintainer_maitain_all(void);
extern PGDLLEXPORT void
pgroonga_standby_maintainer_maintain(Datum datum) pg_attribute_noreturn();
