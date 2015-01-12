/*
 * IDENTIFICATION
 *	gproonga_types.c
 */

#include "pgroonga.h"

#include <catalog/pg_type.h>
#include <utils/builtins.h>

#include <groonga.h>

static int
bpchar_size(const BpChar *arg)
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

PG_FUNCTION_INFO_V1(pgroonga_typeof);
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

/**
 * pgroonga_typeof -- map a postgres' built-in type to a Groonga's type
 *
 * Raises ERROR if no corresponding types found.
 */
Datum
pgroonga_typeof(PG_FUNCTION_ARGS)
{
	Oid		typid = PG_GETARG_OID(0);
	int		typmod = PG_GETARG_INT32(1);
	int32	maxlen;

	/* TODO: support array and record types. */
	switch (typid)
	{
		case BOOLOID:
			return GRN_DB_BOOL;
		case INT2OID:
			return GRN_DB_INT16;
		case INT4OID:
			return GRN_DB_INT32;
		case INT8OID:
			return GRN_DB_INT64;
		case FLOAT4OID:
		case FLOAT8OID:
			return GRN_DB_FLOAT;
		case TIMESTAMPOID:
		case TIMESTAMPTZOID:
#ifdef HAVE_INT64_TIMESTAMP
			return GRN_DB_INT64;	/* FIXME: use GRN_DB_TIME instead */
#else
			return GRN_DB_FLOAT;
#endif
		case TEXTOID:
		case XMLOID:
			return GRN_DB_LONG_TEXT;
		case BPCHAROID:
		case VARCHAROID:
			maxlen = type_maximum_size(typid, typmod);
			if (maxlen >= 0)
			{
				if (maxlen < 4096)
					return GRN_DB_SHORT_TEXT;	/* 4KB */
				if (maxlen < 64 * 1024)
					return GRN_DB_TEXT;			/* 64KB */
			}
			return GRN_DB_LONG_TEXT;
#ifdef NOT_USED
		case POINTOID:
			return GRN_DB_TOKYO_GEO_POINT or GRN_DB_WGS84_GEO_POINT;
#endif
		default:
			ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					errmsg("Groonga: unsupported type: %u", typid)));
			return GRN_DB_VOID;	/* keep compiler quiet */
	}
}

Datum
pgroonga_get_text(PG_FUNCTION_ARGS)
{
	text *var = PG_GETARG_TEXT_PP(0);
	grn_ctx	*ctx = (grn_ctx *)PG_GETARG_POINTER(1);
	grn_obj *buffer = (grn_obj *)PG_GETARG_POINTER(2);

	GRN_TEXT_PUT(ctx, buffer, VARDATA_ANY(var), VARSIZE_ANY_EXHDR(var));

	PG_RETURN_VOID();
}

Datum
pgroonga_get_bpchar(PG_FUNCTION_ARGS)
{
	BpChar *var = PG_GETARG_BPCHAR_PP(0);
	grn_ctx	*ctx = (grn_ctx *)PG_GETARG_POINTER(1);
	grn_obj *buffer = (grn_obj *)PG_GETARG_POINTER(2);

	GRN_TEXT_PUT(ctx, buffer, VARDATA_ANY(var), bpchar_size(var));

	PG_RETURN_VOID();
}

Datum
pgroonga_get_bool(PG_FUNCTION_ARGS)
{
	bool		var = PG_GETARG_BOOL(0);
	int		   *len = (int *) PG_GETARG_POINTER(1);
	const char *ret = (var ? "true" : "false");

	*len = strlen(ret);

	PG_RETURN_POINTER(ret);
}

Datum
pgroonga_get_int2(PG_FUNCTION_ARGS)
{
	int16		var = PG_GETARG_INT16(0);
	int		   *len = (int *) PG_GETARG_POINTER(1);
	char	   *ret = (char *) palloc(8);

	*len = snprintf(ret, 8, "%d", var);

	PG_RETURN_POINTER(ret);
}

Datum
pgroonga_get_int4(PG_FUNCTION_ARGS)
{
	int32		var = PG_GETARG_INT32(0);
	int		   *len = (int *) PG_GETARG_POINTER(1);
	char	   *ret = (char *) palloc(12);

	*len = snprintf(ret, 12, "%d", var);

	PG_RETURN_POINTER(ret);
}

Datum
pgroonga_get_int8(PG_FUNCTION_ARGS)
{
	int64		var = PG_GETARG_INT64(0);
	int		   *len = (int *) PG_GETARG_POINTER(1);
	char	   *ret = (char *) palloc(24);

	*len = snprintf(ret, 24, INT64_FORMAT, var);

	PG_RETURN_POINTER(ret);
}

Datum
pgroonga_get_float4(PG_FUNCTION_ARGS)
{
	float4		var = PG_GETARG_FLOAT4(0);
	int		   *len = (int *) PG_GETARG_POINTER(1);
	char	   *ret = (char *) palloc(32);

	*len = snprintf(ret, 32, "%f", var);

	PG_RETURN_POINTER(ret);
}

Datum
pgroonga_get_float8(PG_FUNCTION_ARGS)
{
	float8		var = PG_GETARG_FLOAT8(0);
	int		   *len = (int *) PG_GETARG_POINTER(1);
	char	   *ret = (char *) palloc(32);

	*len = snprintf(ret, 32, "%f", var);

	PG_RETURN_POINTER(ret);
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

Datum
pgroonga_set_text(PG_FUNCTION_ARGS)
{
	grn_ctx	   *ctx = (grn_ctx *) PG_GETARG_POINTER(0);
	grn_obj	   *obj = (grn_obj *) PG_GETARG_POINTER(1);
	text	   *var = PG_GETARG_TEXT_PP(2);

	GRN_TEXT_SET(ctx, obj, VARDATA_ANY(var), VARSIZE_ANY_EXHDR(var));
	PG_RETURN_VOID();
}

Datum
pgroonga_set_bpchar(PG_FUNCTION_ARGS)
{
	grn_ctx	   *ctx = (grn_ctx *) PG_GETARG_POINTER(0);
	grn_obj	   *obj = (grn_obj *) PG_GETARG_POINTER(1);
	BpChar	   *var = PG_GETARG_BPCHAR_PP(2);

	GRN_TEXT_SET(ctx, obj, VARDATA_ANY(var), bpchar_size(var));
	PG_RETURN_VOID();
}

Datum
pgroonga_set_bool(PG_FUNCTION_ARGS)
{
	grn_ctx	   *ctx = (grn_ctx *) PG_GETARG_POINTER(0);
	grn_obj	   *obj = (grn_obj *) PG_GETARG_POINTER(1);
	bool		var = PG_GETARG_BOOL(2);

	GRN_BOOL_SET(ctx, obj, var);
	PG_RETURN_VOID();
}

Datum
pgroonga_set_int2(PG_FUNCTION_ARGS)
{
	grn_ctx	   *ctx = (grn_ctx *) PG_GETARG_POINTER(0);
	grn_obj	   *obj = (grn_obj *) PG_GETARG_POINTER(1);
	int16		var = PG_GETARG_INT16(2);

	GRN_INT16_SET(ctx, obj, var);
	PG_RETURN_VOID();
}

Datum
pgroonga_set_int4(PG_FUNCTION_ARGS)
{
	grn_ctx	   *ctx = (grn_ctx *) PG_GETARG_POINTER(0);
	grn_obj	   *obj = (grn_obj *) PG_GETARG_POINTER(1);
	int32		var = PG_GETARG_INT32(2);

	GRN_INT32_SET(ctx, obj, var);
	PG_RETURN_VOID();
}

Datum
pgroonga_set_int8(PG_FUNCTION_ARGS)
{
	grn_ctx	   *ctx = (grn_ctx *) PG_GETARG_POINTER(0);
	grn_obj	   *obj = (grn_obj *) PG_GETARG_POINTER(1);
	int64		var = PG_GETARG_INT64(2);

	GRN_INT64_SET(ctx, obj, var);
	PG_RETURN_VOID();
}

Datum
pgroonga_set_float4(PG_FUNCTION_ARGS)
{
	grn_ctx	   *ctx = (grn_ctx *) PG_GETARG_POINTER(0);
	grn_obj	   *obj = (grn_obj *) PG_GETARG_POINTER(1);
	float8		var = (float8) PG_GETARG_FLOAT4(2);

	GRN_FLOAT_SET(ctx, obj, var);
	PG_RETURN_VOID();
}

Datum
pgroonga_set_float8(PG_FUNCTION_ARGS)
{
	grn_ctx	   *ctx = (grn_ctx *) PG_GETARG_POINTER(0);
	grn_obj	   *obj = (grn_obj *) PG_GETARG_POINTER(1);
	float8		var = PG_GETARG_FLOAT8(2);

	GRN_FLOAT_SET(ctx, obj, var);
	PG_RETURN_VOID();
}

Datum
pgroonga_set_timestamp(PG_FUNCTION_ARGS)
{
#ifdef HAVE_INT64_TIMESTAMP
	return pgroonga_set_int8(fcinfo);
#else
	return pgroonga_set_float8(fcinfo);
#endif
}

Datum
pgroonga_set_timestamptz(PG_FUNCTION_ARGS)
{
	return pgroonga_set_timestamp(fcinfo);
}
