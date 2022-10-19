#pragma once

#include "pgrn-compatible.h"

extern PGDLLEXPORT void
pgroonga_standby_maintainer_apply_all(void);
extern PGDLLEXPORT void
pgroonga_standby_maintainer_apply(Datum datum) pg_attribute_noreturn();

