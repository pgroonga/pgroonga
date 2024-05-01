#include "pgroonga.h"

#include <funcapi.h>

PGDLLEXPORT PG_FUNCTION_INFO_V1(pgroonga_list_broken_indexes);

Datum
pgroonga_list_broken_indexes(PG_FUNCTION_ARGS)
{
	FuncCallContext *context;
	if (SRF_IS_FIRSTCALL())
	{
		context = SRF_FIRSTCALL_INIT();
	}
	context = SRF_PERCALL_SETUP();
	// todo
	SRF_RETURN_DONE(context);
}
