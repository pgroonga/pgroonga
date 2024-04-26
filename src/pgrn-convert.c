#include "pgroonga.h"

#include "pgrn-compatible.h"

#include "pgrn-convert.h"
#include "pgrn-global.h"
#include "pgrn-groonga.h"
#include "pgrn-pg.h"

#include <catalog/pg_type.h>
#include <utils/array.h>
#include <utils/builtins.h>
#include <utils/timestamp.h>

static grn_ctx *ctx = &PGrnContext;

static void
PGrnConvertFromDataArrayType(Datum datum,
							 Oid typeID,
							 grn_obj *buffer,
							 const char *tag)
{
	ArrayType *value = DatumGetArrayTypeP(datum);
	ArrayIterator iterator;
	Datum elementDatum;
	bool isNULL;

	if (ARR_NDIM(value) == 0)
		return;

	iterator = array_create_iterator(value, 0, NULL);
	while (array_iterate(iterator, &elementDatum, &isNULL))
	{
		int weight = 0;

		if (isNULL)
		{
			grn_vector_add_element(
				ctx, buffer, NULL, 0, weight, buffer->header.domain);
			continue;
		}

		switch (typeID)
		{
		case INT4ARRAYOID:
		{
			int32 element;
			element = DatumGetInt32(elementDatum);
			GRN_INT32_PUT(ctx, buffer, element);
			break;
		}
		case VARCHARARRAYOID:
		{
			VarChar *element;
			element = DatumGetVarCharPP(elementDatum);
			grn_vector_add_element(ctx,
								   buffer,
								   VARDATA_ANY(element),
								   VARSIZE_ANY_EXHDR(element),
								   weight,
								   buffer->header.domain);
			break;
		}
		case TEXTARRAYOID:
		{
			VarChar *element;
			element = DatumGetTextPP(elementDatum);
			grn_vector_add_element(ctx,
								   buffer,
								   VARDATA_ANY(element),
								   VARSIZE_ANY_EXHDR(element),
								   weight,
								   buffer->header.domain);
			break;
		}
		default:
			PGrnCheckRC(GRN_FUNCTION_NOT_IMPLEMENTED,
						"%s unsupported datum array type: %u",
						tag,
						typeID);
			break;
		}
	}
	array_free_iterator(iterator);
}

void
PGrnConvertFromData(Datum datum, Oid typeID, grn_obj *buffer)
{
	const char *tag = "[data][postgresql->groonga]";

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
			unixTimeLocal = PGrnPGTimestampToLocalTime(value);
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
		GRN_TEXT_SET(ctx, buffer, VARDATA_ANY(value), VARSIZE_ANY_EXHDR(value));
		break;
	}
	case VARCHAROID:
	{
		VarChar *value = DatumGetVarCharPP(datum);
		GRN_TEXT_SET(ctx, buffer, VARDATA_ANY(value), VARSIZE_ANY_EXHDR(value));
		break;
	}
#ifdef NOT_USED
	case POINTOID:
		/* GRN_DB_TOKYO_GEO_POINT or GRN_DB_WGS84_GEO_POINT; */
		break;
#endif
	case INT4ARRAYOID:
	case VARCHARARRAYOID:
	case TEXTARRAYOID:
		PGrnConvertFromDataArrayType(datum, typeID, buffer, tag);
		break;
	case UUIDOID:
	{
		Datum uuidCStringDatum = DirectFunctionCall1(uuid_out, datum);
		GRN_TEXT_SETS(ctx, buffer, DatumGetCString(uuidCStringDatum));
		break;
	}
	default:
		PGrnCheckRC(GRN_FUNCTION_NOT_IMPLEMENTED,
					"%s unsupported datum type: %u",
					tag,
					typeID);
		break;
	}
}
