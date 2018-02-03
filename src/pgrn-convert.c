#include "pgroonga.h"

#include "pgrn-compatible.h"

#include "pgrn-convert.h"
#include "pgrn-global.h"

#include <catalog/pg_type.h>
#include <utils/array.h>
#include <utils/builtins.h>
#include <utils/timestamp.h>

static grn_ctx *ctx = &PGrnContext;

static void
PGrnConvertFromDataArrayType(Datum datum, Oid typeID, grn_obj *buffer)
{
	ArrayType *value = DatumGetArrayTypeP(datum);
	ArrayIterator iterator;
	Datum elementDatum;
	bool isNULL;

	if (ARR_NDIM(value) == 0)
		return;

	iterator = pgrn_array_create_iterator(value, 0);
	while (array_iterate(iterator, &elementDatum, &isNULL))
	{
		int weight = 0;
		VarChar *element;

		if (isNULL)
			continue;

		switch (typeID)
		{
		case VARCHARARRAYOID:
			element = DatumGetVarCharPP(elementDatum);
			grn_vector_add_element(ctx, buffer,
								   VARDATA_ANY(element),
								   VARSIZE_ANY_EXHDR(element),
								   weight,
								   buffer->header.domain);
			break;
		case TEXTARRAYOID:
			element = DatumGetTextPP(elementDatum);
			grn_vector_add_element(ctx, buffer,
								   VARDATA_ANY(element),
								   VARSIZE_ANY_EXHDR(element),
								   weight,
								   buffer->header.domain);
			break;
		}
	}
	array_free_iterator(iterator);
}

void
PGrnConvertFromData(Datum datum, Oid typeID, grn_obj *buffer)
{
	switch (typeID)
	{
	case BOOLOID:
		GRN_BOOL_SET(ctx, buffer, DatumGetBool(datum));
		break;
	case INT2OID:
		GRN_INT16_SET(ctx, buffer, DatumGetInt16(datum));
		break;
	case INT4OID:
		GRN_INT32_SET(ctx, buffer, DatumGetInt32(datum));
		break;
	case INT8OID:
		GRN_INT64_SET(ctx, buffer, DatumGetInt64(datum));
		break;
	case FLOAT4OID:
		GRN_FLOAT_SET(ctx, buffer, DatumGetFloat4(datum));
		break;
	case FLOAT8OID:
		GRN_FLOAT_SET(ctx, buffer, DatumGetFloat8(datum));
		break;
	case TIMESTAMPOID:
	case TIMESTAMPTZOID:
	{
		Timestamp value = DatumGetTimestamp(datum);
		pg_time_t unixTimeLocal;
		int32 usec;

		if (typeID == TIMESTAMPOID)
		{
			long int timezoneOffset;
			timezoneOffset = PGrnPGGetSessionTimezoneOffset();
			unixTimeLocal = timestamptz_to_time_t(value) + timezoneOffset;
		}
		else
		{
			/* TODO: Support not localtime time zone. */
			unixTimeLocal = timestamptz_to_time_t(value);
		}
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
		GRN_TIME_SET(ctx, buffer, GRN_TIME_PACK(unixTimeLocal, usec));
		break;
	}
	case TEXTOID:
	case XMLOID:
	{
		text *value = DatumGetTextPP(datum);
		GRN_TEXT_SET(ctx, buffer,
					 VARDATA_ANY(value), VARSIZE_ANY_EXHDR(value));
		break;
	}
	case VARCHAROID:
	{
		VarChar *value = DatumGetVarCharPP(datum);
		GRN_TEXT_SET(ctx, buffer,
					 VARDATA_ANY(value), VARSIZE_ANY_EXHDR(value));
		break;
	}
#ifdef NOT_USED
	case POINTOID:
		/* GRN_DB_TOKYO_GEO_POINT or GRN_DB_WGS84_GEO_POINT; */
		break;
#endif
	case VARCHARARRAYOID:
	case TEXTARRAYOID:
		PGrnConvertFromDataArrayType(datum, typeID, buffer);
		break;
	default:
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("pgroonga: unsupported datum type: %u",
						typeID)));
		break;
	}
}
