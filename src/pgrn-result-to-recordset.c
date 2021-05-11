#include "pgroonga.h"

#include "pgrn-compatible.h"

#include "pgrn-global.h"
#include "pgrn-groonga.h"
#include "pgrn-pg.h"

#include <funcapi.h>
#include <utils/builtins.h>
#include <utils/jsonb.h>

static grn_ctx *ctx = &PGrnContext;

PGRN_FUNCTION_INFO_V1(pgroonga_result_to_recordset);

typedef struct
{
	FuncCallContext *context;
	Jsonb *jsonb;
	JsonbIterator *iterator;
	TupleDesc desc;
	grn_command_version commandVersion;
} PGrnResultToRecordsetData;

static const char *
PGrnJSONBIteratorTokenToString(JsonbIteratorToken token)
{
	switch (token)
	{
	case WJB_DONE:
		return "done";
	case WJB_KEY:
		return "key";
	case WJB_VALUE:
		return "value";
	case WJB_ELEM:
		return "element";
	case WJB_BEGIN_ARRAY:
		return "begin-array";
	case WJB_END_ARRAY:
		return "end-array";
	case WJB_BEGIN_OBJECT:
		return "begin-object";
	case WJB_END_OBJECT:
		return "end-object";
	default:
		return "unknown";
	}
}

static void
PGrnResultToRecordsetBuildTupleDesc1FillAttribute(
	PGrnResultToRecordsetData *data,
	AttrNumber i)
{
	NameData nameData;
	Oid typeOid;

	{
		JsonbValue name;
		JsonbIteratorToken token;
		token = JsonbIteratorNext(&(data->iterator), &name, false);
		if (token != WJB_ELEM)
		{
			ereport(ERROR,
					(errcode(ERRCODE_DATA_EXCEPTION),
					 errmsg("[pgroonga][result-to-recordset][1][select]"
							"[column][%d] "
							"column name is missing: %s",
							i - 1,
							PGrnJSONBIteratorTokenToString(token))));
		}
		if (name.type != jbvString)
		{
			ereport(ERROR,
					(errcode(ERRCODE_DATA_EXCEPTION),
					 errmsg("[pgroonga][result-to-recordset][1][select]"
							"[column][%d] "
							"column name must be string: %d",
							i - 1,
							name.type)));
		}
		if (name.val.string.len >= NAMEDATALEN - 1)
		{
			ereport(ERROR,
					(errcode(ERRCODE_DATA_EXCEPTION),
					 errmsg("[pgroonga][result-to-recordset][1][select]"
							"[column][%d] "
							"column name is too long: %d: max=%d",
							i - 1,
							name.val.string.len,
							NAMEDATALEN - 1)));
		}
		memcpy(NameStr(nameData), name.val.string.val, name.val.string.len);
		NameStr(nameData)[name.val.string.len] = '\0';
	}

	{
		JsonbValue typeName;
		JsonbIteratorToken token;
		grn_obj *type;
		grn_id typeID;
		token = JsonbIteratorNext(&(data->iterator), &typeName, false);
		if (token != WJB_ELEM)
		{
			ereport(ERROR,
					(errcode(ERRCODE_DATA_EXCEPTION),
					 errmsg("[pgroonga][result-to-recordset][1][select]"
							"[column][%d] "
							"column type is missing: %s",
							i,
							PGrnJSONBIteratorTokenToString(token))));
		}
		if (typeName.type != jbvString)
		{
			ereport(ERROR,
					(errcode(ERRCODE_DATA_EXCEPTION),
					 errmsg("[pgroonga][result-to-recordset][1][select]"
							"[column][%d] "
							"column type must be string: %d",
							i,
							typeName.type)));
		}
		type = grn_ctx_get(ctx,
						   typeName.val.string.val,
						   typeName.val.string.len);
		if (!type)
		{
			ereport(ERROR,
					(errcode(ERRCODE_DATA_EXCEPTION),
					 errmsg("[pgroonga][result-to-recordset][1][select]"
							"[column][%d] "
							"unknown column type: <%.*s>",
							i,
							typeName.val.string.len,
							typeName.val.string.val)));
		}
		typeID = grn_obj_id(ctx, type);
		grn_obj_unref(ctx, type);
		typeOid = PGrnGrnTypeToPGType(typeID);
	}

	TupleDescInitEntry(data->desc,
					   i,
					   NameStr(nameData),
					   typeOid,
					   -1,
					   0);
}

static void
PGrnResultToRecordsetBuildTupleDesc1(PGrnResultToRecordsetData *data)
{
	JsonbValue value;
	JsonbIteratorToken token;
	token = JsonbIteratorNext(&(data->iterator), &value, true);
	if (token != WJB_ELEM)
	{
		ereport(ERROR,
				(errcode(ERRCODE_DATA_EXCEPTION),
				 errmsg("[pgroonga][result-to-recordset][1] "
						"header is missing: %s",
						PGrnJSONBIteratorTokenToString(token))));
	}
	token = JsonbIteratorNext(&(data->iterator), &value, false);
	if (token != WJB_BEGIN_ARRAY)
	{
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("[pgroonga][result-to-recordset][1] "
						"select is only supported: %s",
						PGrnJSONBIteratorTokenToString(token))));
	}
	token = JsonbIteratorNext(&(data->iterator), &value, false);
	if (token != WJB_BEGIN_ARRAY)
	{
		ereport(ERROR,
				(errcode(ERRCODE_DATA_EXCEPTION),
				 errmsg("[pgroonga][result-to-recordset][1][select] "
						"result set is missing: %s",
						PGrnJSONBIteratorTokenToString(token))));
	}
	token = JsonbIteratorNext(&(data->iterator), &value, true);
	if (token != WJB_ELEM)
	{
		ereport(ERROR,
				(errcode(ERRCODE_DATA_EXCEPTION),
				 errmsg("[pgroonga][result-to-recordset][1][select] "
						"the number of hits is missing: %s",
						PGrnJSONBIteratorTokenToString(token))));
	}
	token = JsonbIteratorNext(&(data->iterator), &value, false);
	if (token != WJB_BEGIN_ARRAY)
	{
		ereport(ERROR,
				(errcode(ERRCODE_DATA_EXCEPTION),
				 errmsg("[pgroonga][result-to-recordset][1][select] "
						"column information set must be array: %s",
						PGrnJSONBIteratorTokenToString(token))));
	}

	data->desc = CreateTemplateTupleDesc(value.val.array.nElems);
	{
		AttrNumber i = 1;
		while (true)
		{
			token = JsonbIteratorNext(&(data->iterator), &value, false);
			if (token == WJB_END_ARRAY)
				break;

			if (token != WJB_BEGIN_ARRAY)
			{
				ereport(ERROR,
						(errcode(ERRCODE_DATA_EXCEPTION),
						 errmsg("[pgroonga][result-to-recordset][1][select]"
								"[column][%d] "
								"column information must be array: %s",
								i - 1,
								PGrnJSONBIteratorTokenToString(token))));
			}
			PGrnResultToRecordsetBuildTupleDesc1FillAttribute(data, i);
			i++;
			while (true)
			{
				token = JsonbIteratorNext(&(data->iterator), &value, true);
				if (token == WJB_END_ARRAY)
					break;
			}
		}
	}
}

static void
PGrnResultToRecordsetBuildTupleDesc3FillAttribute(
	PGrnResultToRecordsetData *data,
	AttrNumber i)
{
	bool haveName = false;
	NameData nameData;
	bool haveType = false;
	Oid typeOid = InvalidOid;

	while (true) {
		JsonbValue key;
		grn_raw_string keyString;
		JsonbIteratorToken token;

		token = JsonbIteratorNext(&(data->iterator), &key, false);
		if (token == WJB_END_OBJECT)
			break;
		if (token != WJB_KEY)
		{
			ereport(ERROR,
					(errcode(ERRCODE_DATA_EXCEPTION),
					 errmsg("[pgroonga][result-to-recordset][3][select]"
							"[column][%d] "
							"body.columns key is missing: %s",
							i - 1,
							PGrnJSONBIteratorTokenToString(token))));
		}
		keyString.value = key.val.string.val;
		keyString.length = key.val.string.len;
		if (GRN_RAW_STRING_EQUAL_CSTRING(keyString, "name"))
		{
			JsonbValue value;
			token = JsonbIteratorNext(&(data->iterator), &value, false);
			if (token != WJB_VALUE)
			{
				ereport(ERROR,
						(errcode(ERRCODE_DATA_EXCEPTION),
						 errmsg("[pgroonga][result-to-recordset][3][select]"
								"[column][%d] "
								"body.columns.name must be scalar: %s",
								i - 1,
								PGrnJSONBIteratorTokenToString(token))));
			}
			if (value.type != jbvString)
			{
				ereport(ERROR,
						(errcode(ERRCODE_DATA_EXCEPTION),
						 errmsg("[pgroonga][result-to-recordset][3][select]"
								"[column][%d] "
								"body.columns.name must be string: %d",
								i - 1,
								value.type)));
			}
			if (value.val.string.len >= NAMEDATALEN - 1)
			{
				ereport(ERROR,
						(errcode(ERRCODE_DATA_EXCEPTION),
						 errmsg("[pgroonga][result-to-recordset][3][select]"
								"[column][%d] "
								"body.columns.name is too long: %d: max=%d",
								i - 1,
								value.val.string.len,
								NAMEDATALEN - 1)));
			}
			memcpy(NameStr(nameData),
				   value.val.string.val,
				   value.val.string.len);
			NameStr(nameData)[value.val.string.len] = '\0';
			haveName = true;
		}
		else if (GRN_RAW_STRING_EQUAL_CSTRING(keyString, "type"))
		{
			JsonbValue value;
			grn_obj *type;
			grn_id typeID;
			token = JsonbIteratorNext(&(data->iterator), &value, false);
			if (token != WJB_VALUE)
			{
				ereport(ERROR,
						(errcode(ERRCODE_DATA_EXCEPTION),
						 errmsg("[pgroonga][result-to-recordset][3][select]"
								"[column][%d] "
								"body.columns.type must be scalar: %s",
								i - 1,
								PGrnJSONBIteratorTokenToString(token))));
			}
			if (value.type != jbvString)
			{
				ereport(ERROR,
						(errcode(ERRCODE_DATA_EXCEPTION),
						 errmsg("[pgroonga][result-to-recordset][3][select]"
								"[column][%d] "
								"body.columns.type must be string: %d",
								i - 1,
								value.type)));
			}
			type = grn_ctx_get(ctx,
							   value.val.string.val,
							   value.val.string.len);
			if (!type)
			{
				ereport(ERROR,
						(errcode(ERRCODE_DATA_EXCEPTION),
						 errmsg("[pgroonga][result-to-recordset][3][select]"
								"[column][%d] "
								"unknown column type: <%.*s>",
								i,
								value.val.string.len,
								value.val.string.val)));
			}
			typeID = grn_obj_id(ctx, type);
			grn_obj_unref(ctx, type);
			typeOid = PGrnGrnTypeToPGType(typeID);
			haveType = true;
		}
		else
		{
			JsonbValue value;
			token = JsonbIteratorNext(&(data->iterator), &value, true);
		}
	}

	if (!haveName)
	{
		ereport(ERROR,
				(errcode(ERRCODE_DATA_EXCEPTION),
				 errmsg("[pgroonga][result-to-recordset][3][select]"
						"[column][%d] "
						"column name is missing",
						i)));
	}
	if (!haveType)
	{
		ereport(ERROR,
				(errcode(ERRCODE_DATA_EXCEPTION),
				 errmsg("[pgroonga][result-to-recordset][3][select]"
						"[column][%d] "
						"column type is missing",
						i)));
	}

	TupleDescInitEntry(data->desc,
					   i,
					   NameStr(nameData),
					   typeOid,
					   -1,
					   0);
}

static void
PGrnResultToRecordsetBuildTupleDesc3(PGrnResultToRecordsetData *data)
{
	JsonbIterator *recordsIterator = NULL;

	while (true)
	{
		JsonbValue key;
		grn_raw_string keyString;
		JsonbValue value;
		JsonbIteratorToken token;
		token = JsonbIteratorNext(&(data->iterator), &key, false);
		if (token != WJB_KEY)
		{
			ereport(ERROR,
					(errcode(ERRCODE_DATA_EXCEPTION),
					 errmsg("[pgroonga][result-to-recordset][3] "
							"top-level key is missing: %s",
							PGrnJSONBIteratorTokenToString(token))));
		}
		keyString.value = key.val.string.val;
		keyString.length = key.val.string.len;
		if (!GRN_RAW_STRING_EQUAL_CSTRING(keyString, "body"))
		{
			JsonbIteratorNext(&(data->iterator), &value, true);
			continue;
		}
		token = JsonbIteratorNext(&(data->iterator), &value, false);
		if (token != WJB_BEGIN_OBJECT)
		{
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("[pgroonga][result-to-recordset][3] "
							"select is only supported: %s",
							PGrnJSONBIteratorTokenToString(token))));
		}
		break;
	}
	while (true)
	{
		JsonbValue key;
		grn_raw_string keyString;
		JsonbIteratorToken token;
		token = JsonbIteratorNext(&(data->iterator), &key, false);
		if (token == WJB_END_OBJECT)
			break;
		if (token != WJB_KEY)
		{
			ereport(ERROR,
					(errcode(ERRCODE_DATA_EXCEPTION),
					 errmsg("[pgroonga][result-to-recordset][3][select] "
							"body key is missing: %s",
							PGrnJSONBIteratorTokenToString(token))));
		}
		keyString.value = key.val.string.val;
		keyString.length = key.val.string.len;
		if (GRN_RAW_STRING_EQUAL_CSTRING(keyString, "records"))
		{
			JsonbValue value;
			token = JsonbIteratorNext(&(data->iterator), &value, false);
			if (token != WJB_BEGIN_ARRAY)
			{
				ereport(ERROR,
						(errcode(ERRCODE_DATA_EXCEPTION),
						 errmsg("[pgroonga][result-to-recordset][3][select] "
								"body.records must be array: %s",
								PGrnJSONBIteratorTokenToString(token))));
			}
			recordsIterator = JsonbIteratorInit(data->iterator->container);
			while (true)
			{
				token = JsonbIteratorNext(&(data->iterator), &value, true);
				if (token == WJB_END_ARRAY)
					break;
			}
		}
		else if (GRN_RAW_STRING_EQUAL_CSTRING(keyString, "columns"))
		{
			JsonbValue value;
			token = JsonbIteratorNext(&(data->iterator), &value, false);
			if (token != WJB_BEGIN_ARRAY)
			{
				ereport(ERROR,
						(errcode(ERRCODE_DATA_EXCEPTION),
						 errmsg("[pgroonga][result-to-recordset][3][select] "
								"body.columns must be array: %s",
								PGrnJSONBIteratorTokenToString(token))));
			}
			data->desc = CreateTemplateTupleDesc(value.val.array.nElems);
			{
				AttrNumber i = 1;
				while (true)
				{
					token = JsonbIteratorNext(&(data->iterator), &value, false);
					if (token == WJB_END_ARRAY)
						break;
					if (token != WJB_BEGIN_OBJECT)
					{
						ereport(ERROR,
								(errcode(ERRCODE_DATA_EXCEPTION),
								 errmsg("[pgroonga][result-to-recordset]"
										"[3][select][column][%d] "
										"column information must be object: %s",
										i - 1,
										PGrnJSONBIteratorTokenToString(token))));
					}
					PGrnResultToRecordsetBuildTupleDesc3FillAttribute(data, i);
					i++;
				}
			}
		}
		else
		{
			JsonbValue value;
			JsonbIteratorNext(&(data->iterator), &value, true);
		}
	}
	if (!data->desc)
	{
		ereport(ERROR,
				(errcode(ERRCODE_DATA_EXCEPTION),
				 errmsg("[pgroonga][result-to-recordset][3][select] "
						"body.columns is missing")));
	}
	if (!recordsIterator)
	{
		ereport(ERROR,
				(errcode(ERRCODE_DATA_EXCEPTION),
				 errmsg("[pgroonga][result-to-recordset][3][select] "
						"body.records is missing")));
	}

	{
		JsonbValue value;
		JsonbIteratorToken token;
		data->iterator = recordsIterator;
		token = JsonbIteratorNext(&(data->iterator), &value, false);
		if (token != WJB_BEGIN_ARRAY)
		{
			ereport(ERROR,
					(errcode(ERRCODE_DATA_EXCEPTION),
					 errmsg("[pgroonga][result-to-recordset][3][select] "
							"body.records must be array: %s",
							PGrnJSONBIteratorTokenToString(token))));
		}
	}
}

static void
PGrnResultToRecordsetBuildTupleDesc(PGrnResultToRecordsetData *data)
{
	JsonbValue value;
	JsonbIteratorToken token = JsonbIteratorNext(&(data->iterator), &value, false);
	switch (token)
	{
	case WJB_BEGIN_ARRAY:
		data->commandVersion = GRN_COMMAND_VERSION_1;
		break;
	case WJB_BEGIN_OBJECT:
		data->commandVersion = GRN_COMMAND_VERSION_3;
		break;
	default:
		ereport(ERROR,
				(errcode(ERRCODE_DATA_EXCEPTION),
				 errmsg("[pgroonga][result-to-recordset] "
						"top-level must be array or object: %s",
						PGrnJSONBIteratorTokenToString(token))));
		return;
	}

	if (data->commandVersion == GRN_COMMAND_VERSION_1)
	{
		PGrnResultToRecordsetBuildTupleDesc1(data);
	}
	else
	{
		PGrnResultToRecordsetBuildTupleDesc3(data);
	}
}

static HeapTuple
PGrnResultToRecordsetBuildRecord1(PGrnResultToRecordsetData *data)
{
	JsonbValue record;
	JsonbIteratorToken token;
	token = JsonbIteratorNext(&(data->iterator), &record, false);
	if (token == WJB_END_ARRAY)
		return NULL;
	if (token != WJB_BEGIN_ARRAY)
	{
		ereport(ERROR,
				(errcode(ERRCODE_DATA_EXCEPTION),
				 errmsg("[pgroonga][result-to-recordset][1][select] "
						"record must be array: %s",
						PGrnJSONBIteratorTokenToString(token))));
	}

	{
		int nElements = record.val.array.nElems;
		Datum *values = palloc(sizeof(Datum) * nElements);
		bool *nulls = palloc(sizeof(bool) * nElements);
		int i = 0;
		JsonbValue element;
		while (true)
		{
			token = JsonbIteratorNext(&(data->iterator), &element, false);
			if (token == WJB_END_ARRAY)
				break;
			if (token != WJB_ELEM)
			{
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						 errmsg("[pgroonga][result-to-recordset][1][select] "
								"nested element value isn't supported yet: %s",
								PGrnJSONBIteratorTokenToString(token))));
			}
			switch (element.type)
			{
			case jbvNull:
				values[i] = InvalidOid;
				nulls[i] = true;
				break;
			case jbvString:
				values[i] = PointerGetDatum(
					cstring_to_text_with_len(element.val.string.val,
											 element.val.string.len));
				nulls[i] = false;
				break;
			case jbvNumeric:
				{
					Datum value = NumericGetDatum(element.val.numeric);
					switch (data->context->tuple_desc->attrs[i].atttypid) {
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
							float8 fractionalSeconds =
								unixTime - (int64_t)unixTime;
							int64_t usec =
								(int64_t)(fractionalSeconds * 1000000);
							Timestamp timestamp =
								PGrnPGLocalTimeToTimestamp(unixTime) +
								usec;
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
		return heap_form_tuple(data->context->tuple_desc, values, nulls);
	}
}

static HeapTuple
PGrnResultToRecordsetBuildRecord3(PGrnResultToRecordsetData *data)
{
	return PGrnResultToRecordsetBuildRecord1(data);
}

static HeapTuple
PGrnResultToRecordsetBuildRecord(PGrnResultToRecordsetData *data)
{
	if (data->commandVersion == GRN_COMMAND_VERSION_1)
	{
		return PGrnResultToRecordsetBuildRecord1(data);
	}
	else if (data->commandVersion == GRN_COMMAND_VERSION_3)
	{
		return PGrnResultToRecordsetBuildRecord3(data);
	}
	else
	{
		return NULL;
	}
}

/**
 * pgroonga_result_to_recordset(result jsonb) : SETOF RECORD
 */
Datum
pgroonga_result_to_recordset(PG_FUNCTION_ARGS)
{
	FuncCallContext *context;
	PGrnResultToRecordsetData *data;

	if (SRF_IS_FIRSTCALL())
	{
		MemoryContext oldContext;
		Jsonb *jsonb;
		context = SRF_FIRSTCALL_INIT();
		oldContext = MemoryContextSwitchTo(context->multi_call_memory_ctx);
		jsonb = PG_GETARG_JSONB_P(0);
		data = palloc(sizeof(PGrnResultToRecordsetData));
		context->user_fctx = data;
		data->context = context;
		data->jsonb = jsonb;
		data->iterator = JsonbIteratorInit(&(data->jsonb->root));
		data->desc = NULL;
		PG_TRY();
		{
			PGrnResultToRecordsetBuildTupleDesc(data);
		}
		PG_FINALLY();
		{
			MemoryContextSwitchTo(oldContext);
		}
		PG_END_TRY();
		context->tuple_desc = BlessTupleDesc(data->desc);
	}

	context = SRF_PERCALL_SETUP();
	data = context->user_fctx;

	{
		HeapTuple tuple = PGrnResultToRecordsetBuildRecord(data);
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
