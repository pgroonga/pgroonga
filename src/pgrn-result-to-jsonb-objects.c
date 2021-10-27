#include "pgroonga.h"

#include "pgrn-compatible.h"

#include "pgrn-result-converter.h"

PGDLLEXPORT PG_FUNCTION_INFO_V1(pgroonga_result_to_jsonb_objects);

/**
 * pgroonga_result_to_jsonb_objects(result jsonb) : jsonb
 */
Datum
pgroonga_result_to_jsonb_objects(PG_FUNCTION_ARGS)
{
	Jsonb *jsonb = PG_GETARG_JSONB_P(0);
	PGrnResultConverter converter;
	Jsonb *jsonbObjects;

	PGrnResultConverterInit(&converter, jsonb, "[result-to-jsonb-objects]");
	jsonbObjects = PGrnResultConverterBuildJSONBObjects(&converter);
	PG_RETURN_JSONB_P(jsonbObjects);
}
