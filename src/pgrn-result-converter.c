#include "pgroonga.h"

#include "pgrn-groonga.h"
#include "pgrn-jsonb.h"
#include "pgrn-pg.h"
#include "pgrn-result-converter.h"

#include <access/htup_details.h>
#include <access/tupdesc.h>
#include <catalog/pg_type.h>
#include <funcapi.h>
#include <utils/builtins.h>

void
PGrnResultConverterInit(PGrnResultConverter *converter,
						Jsonb *jsonb,
						const char *tag)
{
	converter->tag = tag;
	converter->iterator = JsonbIteratorInit(&(jsonb->root));
	converter->commandVersion = GRN_COMMAND_VERSION_DEFAULT;
	converter->desc = NULL;
}

static void
PGrnResultConverterDetectCommandVersion(PGrnResultConverter *converter)
{
	JsonbValue value;
	JsonbIteratorToken token;
	token = JsonbIteratorNext(&(converter->iterator), &value, false);
	switch (token)
	{
	case WJB_BEGIN_ARRAY:
		converter->commandVersion = GRN_COMMAND_VERSION_1;
		break;
	case WJB_BEGIN_OBJECT:
		converter->commandVersion = GRN_COMMAND_VERSION_3;
		break;
	default:
		PGrnCheckRC(GRN_INVALID_ARGUMENT,
					"%s top-level must be array or object: %s",
					converter->tag,
					PGrnJSONBIteratorTokenToString(token));
		break;
	}
}

static void
PGrnResultConverterBuildTupleDesc1FillAttribute(PGrnResultConverter *converter,
												AttrNumber i)
{
	NameData nameData;
	Oid typeOid;

	{
		JsonbValue name;
		JsonbIteratorToken token;
		token = JsonbIteratorNext(&(converter->iterator), &name, false);
		if (token != WJB_ELEM)
		{
			PGrnCheckRC(GRN_INVALID_ARGUMENT,
						"%s[1][select][column][%d] "
						"column name is missing: %s",
						converter->tag,
						i - 1,
						PGrnJSONBIteratorTokenToString(token));
		}
		if (name.type != jbvString)
		{
			PGrnCheckRC(GRN_INVALID_ARGUMENT,
						"%s[1][select][column][%d] "
						"column name must be string: %d",
						converter->tag,
						i - 1,
						name.type);
		}
		if (name.val.string.len >= NAMEDATALEN - 1)
		{
			PGrnCheckRC(GRN_INVALID_ARGUMENT,
						"%s[1][select][column][%d] "
						"column name is too long: %d: max=%d",
						converter->tag,
						i - 1,
						name.val.string.len,
						NAMEDATALEN - 1);
		}
		memcpy(NameStr(nameData), name.val.string.val, name.val.string.len);
		NameStr(nameData)[name.val.string.len] = '\0';
	}

	{
		JsonbValue typeName;
		JsonbIteratorToken token;
		grn_obj *type;
		grn_id typeID;
		token = JsonbIteratorNext(&(converter->iterator), &typeName, false);
		if (token != WJB_ELEM)
		{
			PGrnCheckRC(GRN_INVALID_ARGUMENT,
						"%s[1][select][column][%d] "
						"column type is missing: %s",
						converter->tag,
						i,
						PGrnJSONBIteratorTokenToString(token));
		}
		if (typeName.type != jbvString)
		{
			PGrnCheckRC(GRN_INVALID_ARGUMENT,
						"%s[1][select][column][%d] "
						"column type must be string: %d",
						converter->tag,
						i,
						typeName.type);
		}
		type =
			grn_ctx_get(ctx, typeName.val.string.val, typeName.val.string.len);
		if (!type)
		{
			PGrnCheckRC(GRN_INVALID_ARGUMENT,
						"%s[1][select][column][%d] "
						"unknown column type: <%.*s>",
						converter->tag,
						i,
						typeName.val.string.len,
						typeName.val.string.val);
		}
		typeID = grn_obj_id(ctx, type);
		grn_obj_unref(ctx, type);
		typeOid = PGrnGrnTypeToPGType(typeID);
	}

	TupleDescInitEntry(converter->desc, i, NameStr(nameData), typeOid, -1, 0);
}

static void
PGrnResultConverterBuildTupleDesc1(PGrnResultConverter *converter)
{
	JsonbValue value;
	JsonbIteratorToken token;
	token = JsonbIteratorNext(&(converter->iterator), &value, true);
	if (token != WJB_ELEM)
	{
		PGrnCheckRC(GRN_INVALID_ARGUMENT,
					"%s[1] header is missing: %s",
					converter->tag,
					PGrnJSONBIteratorTokenToString(token));
	}
	token = JsonbIteratorNext(&(converter->iterator), &value, false);
	if (token != WJB_BEGIN_ARRAY)
	{
		PGrnCheckRC(GRN_FUNCTION_NOT_IMPLEMENTED,
					"%s[1] select is only supported: %s",
					converter->tag,
					PGrnJSONBIteratorTokenToString(token));
	}
	token = JsonbIteratorNext(&(converter->iterator), &value, false);
	if (token != WJB_BEGIN_ARRAY)
	{
		PGrnCheckRC(GRN_INVALID_ARGUMENT,
					"%s[1][select] "
					"result set is missing: %s",
					converter->tag,
					PGrnJSONBIteratorTokenToString(token));
	}
	token = JsonbIteratorNext(&(converter->iterator), &value, true);
	if (token != WJB_ELEM)
	{
		PGrnCheckRC(GRN_INVALID_ARGUMENT,
					"%s[1][select] "
					"the number of hits is missing: %s",
					converter->tag,
					PGrnJSONBIteratorTokenToString(token));
	}
	token = JsonbIteratorNext(&(converter->iterator), &value, false);
	if (token != WJB_BEGIN_ARRAY)
	{
		PGrnCheckRC(GRN_INVALID_ARGUMENT,
					"%s[1][select] "
					"column information set must be array: %s",
					converter->tag,
					PGrnJSONBIteratorTokenToString(token));
	}

	converter->desc = CreateTemplateTupleDesc(value.val.array.nElems);
	{
		AttrNumber i = 1;
		while (true)
		{
			token = JsonbIteratorNext(&(converter->iterator), &value, false);
			if (token == WJB_END_ARRAY)
				break;

			if (token != WJB_BEGIN_ARRAY)
			{
				PGrnCheckRC(GRN_INVALID_ARGUMENT,
							"%s[1][select][column][%d] "
							"column information must be array: %s",
							converter->tag,
							i - 1,
							PGrnJSONBIteratorTokenToString(token));
			}
			PGrnResultConverterBuildTupleDesc1FillAttribute(converter, i);
			i++;
			while (true)
			{
				token = JsonbIteratorNext(&(converter->iterator), &value, true);
				if (token == WJB_END_ARRAY)
					break;
			}
		}
	}
}

static void
PGrnResultConverterBuildTupleDesc3FillAttribute(PGrnResultConverter *converter,
												AttrNumber i)
{
	bool haveName = false;
	NameData nameData;
	bool haveType = false;
	Oid typeOid = InvalidOid;

	while (true)
	{
		JsonbValue key;
		grn_raw_string keyString;
		JsonbIteratorToken token;

		token = JsonbIteratorNext(&(converter->iterator), &key, false);
		if (token == WJB_END_OBJECT)
			break;
		if (token != WJB_KEY)
		{
			PGrnCheckRC(GRN_INVALID_ARGUMENT,
						"%s[3][select][column][%d] "
						"body.columns key is missing: %s",
						converter->tag,
						i - 1,
						PGrnJSONBIteratorTokenToString(token));
		}
		keyString.value = key.val.string.val;
		keyString.length = key.val.string.len;
		if (GRN_RAW_STRING_EQUAL_CSTRING(keyString, "name"))
		{
			JsonbValue value;
			token = JsonbIteratorNext(&(converter->iterator), &value, false);
			if (token != WJB_VALUE)
			{
				PGrnCheckRC(GRN_INVALID_ARGUMENT,
							"%s[3][select][column][%d] "
							"body.columns.name must be scalar: %s",
							converter->tag,
							i - 1,
							PGrnJSONBIteratorTokenToString(token));
			}
			if (value.type != jbvString)
			{
				PGrnCheckRC(GRN_INVALID_ARGUMENT,
							"%s[3][select][column][%d] "
							"body.columns.name must be string: %d",
							converter->tag,
							i - 1,
							value.type);
			}
			if (value.val.string.len >= NAMEDATALEN - 1)
			{
				PGrnCheckRC(GRN_INVALID_ARGUMENT,
							"%s[3][select][column][%d] "
							"body.columns.name is too long: %d: max=%d",
							converter->tag,
							i - 1,
							value.val.string.len,
							NAMEDATALEN - 1);
			}
			memcpy(
				NameStr(nameData), value.val.string.val, value.val.string.len);
			NameStr(nameData)[value.val.string.len] = '\0';
			haveName = true;
		}
		else if (GRN_RAW_STRING_EQUAL_CSTRING(keyString, "type"))
		{
			JsonbValue value;
			grn_obj *type;
			grn_id typeID;
			token = JsonbIteratorNext(&(converter->iterator), &value, false);
			if (token != WJB_VALUE)
			{
				PGrnCheckRC(GRN_INVALID_ARGUMENT,
							"%s[3][select][column][%d] "
							"body.columns.type must be scalar: %s",
							converter->tag,
							i - 1,
							PGrnJSONBIteratorTokenToString(token));
			}
			if (value.type != jbvString)
			{
				PGrnCheckRC(GRN_INVALID_ARGUMENT,
							"%s[3][select][column][%d] "
							"body.columns.type must be string: %d",
							converter->tag,
							i - 1,
							value.type);
			}
			type = grn_ctx_get(ctx, value.val.string.val, value.val.string.len);
			if (!type)
			{
				PGrnCheckRC(GRN_INVALID_ARGUMENT,
							"%s[3][select][column][%d] "
							"unknown column type: <%.*s>",
							converter->tag,
							i,
							value.val.string.len,
							value.val.string.val);
			}
			typeID = grn_obj_id(ctx, type);
			grn_obj_unref(ctx, type);
			typeOid = PGrnGrnTypeToPGType(typeID);
			haveType = true;
		}
		else
		{
			JsonbValue value;
			token = JsonbIteratorNext(&(converter->iterator), &value, true);
		}
	}

	if (!haveName)
	{
		PGrnCheckRC(GRN_INVALID_ARGUMENT,
					"%s[3][select][column][%d] "
					"column name is missing",
					converter->tag,
					i);
	}
	if (!haveType)
	{
		PGrnCheckRC(GRN_INVALID_ARGUMENT,
					"%s[3][select][column][%d] "
					"column type is missing",
					converter->tag,
					i);
	}

	TupleDescInitEntry(converter->desc, i, NameStr(nameData), typeOid, -1, 0);
}

static void
PGrnResultConverterBuildTupleDesc3(PGrnResultConverter *converter)
{
	JsonbIterator *recordsIterator = NULL;

	while (true)
	{
		JsonbValue key;
		grn_raw_string keyString;
		JsonbValue value;
		JsonbIteratorToken token;
		token = JsonbIteratorNext(&(converter->iterator), &key, false);
		if (token != WJB_KEY)
		{
			PGrnCheckRC(GRN_INVALID_ARGUMENT,
						"%s[3] top-level key is missing: %s",
						converter->tag,
						PGrnJSONBIteratorTokenToString(token));
		}
		keyString.value = key.val.string.val;
		keyString.length = key.val.string.len;
		if (!GRN_RAW_STRING_EQUAL_CSTRING(keyString, "body"))
		{
			JsonbIteratorNext(&(converter->iterator), &value, true);
			continue;
		}
		token = JsonbIteratorNext(&(converter->iterator), &value, false);
		if (token != WJB_BEGIN_OBJECT)
		{
			PGrnCheckRC(GRN_FUNCTION_NOT_IMPLEMENTED,
						"%s[3] select is only supported: %s",
						converter->tag,
						PGrnJSONBIteratorTokenToString(token));
		}
		break;
	}
	while (true)
	{
		JsonbValue key;
		grn_raw_string keyString;
		JsonbIteratorToken token;
		token = JsonbIteratorNext(&(converter->iterator), &key, false);
		if (token == WJB_END_OBJECT)
			break;
		if (token != WJB_KEY)
		{
			PGrnCheckRC(GRN_INVALID_ARGUMENT,
						"%s[3][select] body key is missing: %s",
						converter->tag,
						PGrnJSONBIteratorTokenToString(token));
		}
		keyString.value = key.val.string.val;
		keyString.length = key.val.string.len;
		if (GRN_RAW_STRING_EQUAL_CSTRING(keyString, "records"))
		{
			JsonbValue value;
			token = JsonbIteratorNext(&(converter->iterator), &value, false);
			if (token != WJB_BEGIN_ARRAY)
			{
				PGrnCheckRC(GRN_INVALID_ARGUMENT,
							"%s[3][select] body.records must be array: %s",
							converter->tag,
							PGrnJSONBIteratorTokenToString(token));
			}
			recordsIterator = JsonbIteratorInit(converter->iterator->container);
			while (true)
			{
				token = JsonbIteratorNext(&(converter->iterator), &value, true);
				if (token == WJB_END_ARRAY)
					break;
			}
		}
		else if (GRN_RAW_STRING_EQUAL_CSTRING(keyString, "columns"))
		{
			JsonbValue value;
			token = JsonbIteratorNext(&(converter->iterator), &value, false);
			if (token != WJB_BEGIN_ARRAY)
			{
				PGrnCheckRC(GRN_INVALID_ARGUMENT,
							"%s[3][select] body.columns must be array: %s",
							converter->tag,
							PGrnJSONBIteratorTokenToString(token));
			}
			converter->desc = CreateTemplateTupleDesc(value.val.array.nElems);
			{
				AttrNumber i = 1;
				while (true)
				{
					token = JsonbIteratorNext(
						&(converter->iterator), &value, false);
					if (token == WJB_END_ARRAY)
						break;
					if (token != WJB_BEGIN_OBJECT)
					{
						PGrnCheckRC(GRN_INVALID_ARGUMENT,
									"%s[3][select][column][%d] "
									"column information must be object: %s",
									converter->tag,
									i - 1,
									PGrnJSONBIteratorTokenToString(token));
					}
					PGrnResultConverterBuildTupleDesc3FillAttribute(converter,
																	i);
					i++;
				}
			}
		}
		else
		{
			JsonbValue value;
			JsonbIteratorNext(&(converter->iterator), &value, true);
		}
	}
	if (!converter->desc)
	{
		PGrnCheckRC(GRN_INVALID_ARGUMENT,
					"%s[3][select] body.columns is missing",
					converter->tag);
	}
	if (!recordsIterator)
	{
		PGrnCheckRC(GRN_INVALID_ARGUMENT,
					"%s[3][select] body.records is missing",
					converter->tag);
	}

	{
		JsonbValue value;
		JsonbIteratorToken token;
		converter->iterator = recordsIterator;
		token = JsonbIteratorNext(&(converter->iterator), &value, false);
		if (token != WJB_BEGIN_ARRAY)
		{
			PGrnCheckRC(GRN_INVALID_ARGUMENT,
						"%s[3][select] body.records must be array: %s",
						converter->tag,
						PGrnJSONBIteratorTokenToString(token));
		}
	}
}

void
PGrnResultConverterBuildTupleDesc(PGrnResultConverter *converter)
{
	PGrnResultConverterDetectCommandVersion(converter);
	if (converter->commandVersion == GRN_COMMAND_VERSION_1)
	{
		PGrnResultConverterBuildTupleDesc1(converter);
	}
	else
	{
		PGrnResultConverterBuildTupleDesc3(converter);
	}
	converter->desc = BlessTupleDesc(converter->desc);
}

HeapTuple
PGrnResultConverterBuildTuple(PGrnResultConverter *converter)
{
	JsonbValue record;
	JsonbIteratorToken token;
	token = JsonbIteratorNext(&(converter->iterator), &record, false);
	if (token == WJB_END_ARRAY)
		return NULL;
	if (token != WJB_BEGIN_ARRAY)
	{
		PGrnCheckRC(GRN_INVALID_ARGUMENT,
					"%s[%d][select] record must be array: %s",
					converter->tag,
					converter->commandVersion,
					PGrnJSONBIteratorTokenToString(token));
	}

	{
		int nElements = record.val.array.nElems;
		Datum *values = palloc(sizeof(Datum) * nElements);
		bool *nulls = palloc(sizeof(bool) * nElements);
		int i = 0;
		JsonbValue element;
		while (true)
		{
			token = JsonbIteratorNext(&(converter->iterator), &element, false);
			if (token == WJB_END_ARRAY)
				break;
			if (token != WJB_ELEM)
			{
				PGrnCheckRC(GRN_FUNCTION_NOT_IMPLEMENTED,
							"%s[%d][select] "
							"nested element value isn't supported yet: %s",
							converter->tag,
							converter->commandVersion,
							PGrnJSONBIteratorTokenToString(token));
			}
			switch (element.type)
			{
			case jbvNull:
				values[i] = InvalidOid;
				nulls[i] = true;
				break;
			case jbvString:
				values[i] = PointerGetDatum(cstring_to_text_with_len(
					element.val.string.val, element.val.string.len));
				nulls[i] = false;
				break;
			case jbvNumeric:
			{
				Datum value = NumericGetDatum(element.val.numeric);
				switch (TupleDescAttr(converter->desc, i)->atttypid)
				{
				case INT2OID:
					values[i] = DirectFunctionCall1(numeric_int2, value);
					break;
				case INT4OID:
					values[i] = DirectFunctionCall1(numeric_int4, value);
					break;
				case INT8OID:
					values[i] = DirectFunctionCall1(numeric_int8, value);
					break;
				case FLOAT4OID:
					values[i] = DirectFunctionCall1(numeric_float4, value);
					break;
				case FLOAT8OID:
					values[i] = DirectFunctionCall1(numeric_float8, value);
					break;
				case TIMESTAMPOID:
				{
					Datum unixTimeDatum =
						DirectFunctionCall1(numeric_float8, value);
					float8 unixTime = DatumGetFloat8(unixTimeDatum);
					float8 fractionalSeconds = unixTime - (int64_t) unixTime;
					int64_t usec = (int64_t) (fractionalSeconds * 1000000);
					Timestamp timestamp =
						PGrnPGLocalTimeToTimestamp(unixTime) + usec;
					values[i] = TimestampGetDatum(timestamp);
				}
				break;
				default:
					break;
				}
			}
				nulls[i] = false;
				break;
			case jbvBool:
				values[i] = BoolGetDatum(element.val.boolean);
				nulls[i] = false;
				break;
			default:
				break;
			}
			i++;
		}
		return heap_form_tuple(converter->desc, values, nulls);
	}
}

Jsonb *
PGrnResultConverterBuildJSONBObjects(PGrnResultConverter *converter)
{
#if PG_VERSION_NUM >= 190000
	PGrnJsonState state = {0};
#else
	PGrnJsonState state = NULL;
#endif
	PGrnResultConverterBuildTupleDesc(converter);

	pushJsonbValue(&state, WJB_BEGIN_ARRAY, NULL);
	while (true)
	{
		JsonbValue record;
		JsonbIteratorToken token;
		int i = 0;
		JsonbValue element;

		token = JsonbIteratorNext(&(converter->iterator), &record, false);
		if (token == WJB_END_ARRAY)
			break;
		if (token != WJB_BEGIN_ARRAY)
		{
			PGrnCheckRC(GRN_INVALID_ARGUMENT,
						"%s[%d][select] record must be array: %s",
						converter->tag,
						converter->commandVersion,
						PGrnJSONBIteratorTokenToString(token));
		}

		pushJsonbValue(&state, WJB_BEGIN_OBJECT, NULL);
		while (true)
		{
			token = JsonbIteratorNext(&(converter->iterator), &element, false);
			if (token == WJB_END_ARRAY)
				break;
			if (token != WJB_ELEM)
			{
				PGrnCheckRC(GRN_FUNCTION_NOT_IMPLEMENTED,
							"%s[%d][select] "
							"nested element value isn't supported yet: "
							"%s",
							converter->tag,
							converter->commandVersion,
							PGrnJSONBIteratorTokenToString(token));
			}
			{
				Form_pg_attribute attribute = TupleDescAttr(converter->desc, i);
				JsonbValue key;
				key.type = jbvString;
				key.val.string.val = NameStr(attribute->attname);
				key.val.string.len = strlen(key.val.string.val);
				pushJsonbValue(&state, WJB_KEY, &key);
			}
			pushJsonbValue(&state, WJB_VALUE, &element);
			i++;
		}
		pushJsonbValue(&state, WJB_END_OBJECT, NULL);
	}

	{
		PGrnJsonbValueToJsonb(state);
	}
}
