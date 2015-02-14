/*
 * IDENTIFICATION
 *	gproonga_types.c
 */

#include "pgroonga.h"

#include <catalog/pg_type.h>
#include <utils/builtins.h>

#include <groonga.h>

int
pgroonga_bpchar_size(const BpChar *arg)
{
	char	   *s = VARDATA_ANY(arg);
	int			i;
	int			len;

	len = VARSIZE_ANY_EXHDR(arg);
	for (i = len - 1; i >= 0; i--)
	{
		if (s[i] != ' ')
			break;
	}
	return i + 1;
}

PG_FUNCTION_INFO_V1(pgroonga_get_text);
PG_FUNCTION_INFO_V1(pgroonga_get_bpchar);
PG_FUNCTION_INFO_V1(pgroonga_get_bool);
PG_FUNCTION_INFO_V1(pgroonga_get_int2);
PG_FUNCTION_INFO_V1(pgroonga_get_int4);
PG_FUNCTION_INFO_V1(pgroonga_get_int8);
PG_FUNCTION_INFO_V1(pgroonga_get_float4);
PG_FUNCTION_INFO_V1(pgroonga_get_float8);
PG_FUNCTION_INFO_V1(pgroonga_get_timestamp);
PG_FUNCTION_INFO_V1(pgroonga_get_timestamptz);
PG_FUNCTION_INFO_V1(pgroonga_set_text);
PG_FUNCTION_INFO_V1(pgroonga_set_bpchar);
PG_FUNCTION_INFO_V1(pgroonga_set_bool);
PG_FUNCTION_INFO_V1(pgroonga_set_int2);
PG_FUNCTION_INFO_V1(pgroonga_set_int4);
PG_FUNCTION_INFO_V1(pgroonga_set_int8);
PG_FUNCTION_INFO_V1(pgroonga_set_float4);
PG_FUNCTION_INFO_V1(pgroonga_set_float8);
PG_FUNCTION_INFO_V1(pgroonga_set_timestamp);
PG_FUNCTION_INFO_V1(pgroonga_set_timestamptz);

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
pgroonga_get_bpchar(PG_FUNCTION_ARGS)
{
	grn_ctx	   *ctx = (grn_ctx *) PG_GETARG_POINTER(0);
	grn_obj	   *obj = (grn_obj *) PG_GETARG_POINTER(1);
	BpChar	   *var = PG_GETARG_BPCHAR_PP(2);

	GRN_TEXT_SET(ctx, obj, VARDATA_ANY(var), pgroonga_bpchar_size(var));
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
#ifdef HAVE_INT64_TIMESTAMP
	return pgroonga_get_int8(fcinfo);
#else
	return pgroonga_get_float8(fcinfo);
#endif
}

Datum
pgroonga_get_timestamptz(PG_FUNCTION_ARGS)
{
	return pgroonga_get_timestamp(fcinfo);
}
