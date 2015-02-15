/*
 * IDENTIFICATION
 *	gproonga_types.c
 */

#include "pgroonga.h"

#include <catalog/pg_type.h>
#include <utils/array.h>
#include <utils/builtins.h>
#include <utils/timestamp.h>

#include <groonga.h>

#include <math.h>

PG_FUNCTION_INFO_V1(pgroonga_get_text);
PG_FUNCTION_INFO_V1(pgroonga_get_text_array);
PG_FUNCTION_INFO_V1(pgroonga_get_varchar);
PG_FUNCTION_INFO_V1(pgroonga_get_bool);
PG_FUNCTION_INFO_V1(pgroonga_get_int2);
PG_FUNCTION_INFO_V1(pgroonga_get_int4);
PG_FUNCTION_INFO_V1(pgroonga_get_int8);
PG_FUNCTION_INFO_V1(pgroonga_get_float4);
PG_FUNCTION_INFO_V1(pgroonga_get_float8);
PG_FUNCTION_INFO_V1(pgroonga_get_timestamp);
PG_FUNCTION_INFO_V1(pgroonga_get_timestamptz);

Datum
pgroonga_get_text(PG_FUNCTION_ARGS)
{
	grn_ctx	   *ctx = (grn_ctx *) PG_GETARG_POINTER(0);
	grn_obj	   *obj = (grn_obj *) PG_GETARG_POINTER(1);
	text	   *var = PG_GETARG_TEXT_PP(2);

	GRN_TEXT_SET(ctx, obj, VARDATA_ANY(var), VARSIZE_ANY_EXHDR(var));
	PG_RETURN_VOID();
}

Datum
pgroonga_get_text_array(PG_FUNCTION_ARGS)
{
	grn_ctx	*ctx = (grn_ctx *) PG_GETARG_POINTER(0);
	grn_obj	*obj = (grn_obj *) PG_GETARG_POINTER(1);
	ArrayType *value = PG_GETARG_ARRAYTYPE_P(2);
	int i, n;

	n = ARR_DIMS(value)[0];
	for (i = 1; i <= n; i++)
	{
		int weight = 0;
		Datum elementDatum;
		text *element;
		bool isNULL;

		elementDatum = array_ref(value, 1, &i, -1, -1, false, 'i', &isNULL);
		if (isNULL)
			continue;

		element = DatumGetTextPP(elementDatum);
		grn_vector_add_element(ctx, obj,
							   VARDATA_ANY(element),
							   VARSIZE_ANY_EXHDR(element),
							   weight,
							   obj->header.domain);
	}

	PG_RETURN_VOID();
}

Datum
pgroonga_get_varchar(PG_FUNCTION_ARGS)
{
	grn_ctx	   *ctx = (grn_ctx *) PG_GETARG_POINTER(0);
	grn_obj	   *obj = (grn_obj *) PG_GETARG_POINTER(1);
	VarChar	   *var = PG_GETARG_VARCHAR_PP(2);

	GRN_TEXT_SET(ctx, obj, VARDATA_ANY(var), VARSIZE_ANY_EXHDR(var));
	PG_RETURN_VOID();
}

Datum
pgroonga_get_bool(PG_FUNCTION_ARGS)
{
	grn_ctx	   *ctx = (grn_ctx *) PG_GETARG_POINTER(0);
	grn_obj	   *obj = (grn_obj *) PG_GETARG_POINTER(1);
	bool		var = PG_GETARG_BOOL(2);

	GRN_BOOL_SET(ctx, obj, var);
	PG_RETURN_VOID();
}

Datum
pgroonga_get_int2(PG_FUNCTION_ARGS)
{
	grn_ctx	   *ctx = (grn_ctx *) PG_GETARG_POINTER(0);
	grn_obj	   *obj = (grn_obj *) PG_GETARG_POINTER(1);
	int16		var = PG_GETARG_INT16(2);

	GRN_INT16_SET(ctx, obj, var);
	PG_RETURN_VOID();
}

Datum
pgroonga_get_int4(PG_FUNCTION_ARGS)
{
	grn_ctx	   *ctx = (grn_ctx *) PG_GETARG_POINTER(0);
	grn_obj	   *obj = (grn_obj *) PG_GETARG_POINTER(1);
	int32		var = PG_GETARG_INT32(2);

	GRN_INT32_SET(ctx, obj, var);
	PG_RETURN_VOID();
}

Datum
pgroonga_get_int8(PG_FUNCTION_ARGS)
{
	grn_ctx	   *ctx = (grn_ctx *) PG_GETARG_POINTER(0);
	grn_obj	   *obj = (grn_obj *) PG_GETARG_POINTER(1);
	int64		var = PG_GETARG_INT64(2);

	GRN_INT64_SET(ctx, obj, var);
	PG_RETURN_VOID();
}

Datum
pgroonga_get_float4(PG_FUNCTION_ARGS)
{
	grn_ctx	   *ctx = (grn_ctx *) PG_GETARG_POINTER(0);
	grn_obj	   *obj = (grn_obj *) PG_GETARG_POINTER(1);
	float8		var = (float8) PG_GETARG_FLOAT4(2);

	GRN_FLOAT_SET(ctx, obj, var);
	PG_RETURN_VOID();
}

Datum
pgroonga_get_float8(PG_FUNCTION_ARGS)
{
	grn_ctx	   *ctx = (grn_ctx *) PG_GETARG_POINTER(0);
	grn_obj	   *obj = (grn_obj *) PG_GETARG_POINTER(1);
	float8		var = PG_GETARG_FLOAT8(2);

	GRN_FLOAT_SET(ctx, obj, var);
	PG_RETURN_VOID();
}

Datum
pgroonga_get_timestamp(PG_FUNCTION_ARGS)
{
	grn_ctx	*ctx = (grn_ctx *) PG_GETARG_POINTER(0);
	grn_obj	*obj = (grn_obj *) PG_GETARG_POINTER(1);
	Timestamp value = PG_GETARG_TIMESTAMP(2);
	pg_time_t unixTime;
	int32 usec;

	unixTime = timestamptz_to_time_t(value);
#ifdef HAVE_INT64_TIMESTAMP
	usec = value % USECS_PER_SEC;
#else
	{
		double rawUsec;
		modf(value, &rawUsec);
		usec = rawUsec * USECS_PER_SEC;
		if (usec < 0.0)
		{
			usec = -usec;
		}
	}
#endif
	GRN_TIME_SET(ctx, obj, GRN_TIME_PACK(unixTime, usec));

	PG_RETURN_VOID();
}

Datum
pgroonga_get_timestamptz(PG_FUNCTION_ARGS)
{
	return pgroonga_get_timestamp(fcinfo);
}
