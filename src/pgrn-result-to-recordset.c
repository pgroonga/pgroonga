#include "pgroonga.h"

#include "pgrn-compatible.h"

#include "pgrn-result-converter.h"

#include <funcapi.h>

PGRN_FUNCTION_INFO_V1(pgroonga_result_to_recordset);

/**
 * pgroonga_result_to_recordset(result jsonb) : SETOF RECORD
 */
Datum
pgroonga_result_to_recordset(PG_FUNCTION_ARGS)
{
	FuncCallContext *context;
	PGrnResultConverter *converter;

	if (SRF_IS_FIRSTCALL())
	{
		MemoryContext oldContext;
		Jsonb *jsonb;
		context = SRF_FIRSTCALL_INIT();
		oldContext = MemoryContextSwitchTo(context->multi_call_memory_ctx);
		jsonb = PG_GETARG_JSONB_P(0);
		converter = palloc(sizeof(PGrnResultConverter));
		context->user_fctx = converter;
		PGrnResultConverterInit(converter, jsonb, "[result-to-recordset]");
		PG_TRY();
		{
			PGrnResultConverterBuildTupleDesc(converter);
		}
		PG_CATCH();
		{
			MemoryContextSwitchTo(oldContext);
			PG_RE_THROW();
		}
		PG_END_TRY();
		MemoryContextSwitchTo(oldContext);
		context->tuple_desc = converter->desc;
	}

	context = SRF_PERCALL_SETUP();
	converter = context->user_fctx;

	{
		HeapTuple tuple = PGrnResultConverterBuildTuple(converter);
		if (tuple)
		{
			SRF_RETURN_NEXT(context, HeapTupleGetDatum(tuple));
		}
		else
		{
			SRF_RETURN_DONE(context);
		}
	}
}
