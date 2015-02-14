/*
 * IDENTIFICATION
 *	pgroonga.c
 */

#include "pgroonga.h"

#include <access/reloptions.h>
#include <access/relscan.h>
#include <catalog/catalog.h>
#include <catalog/index.h>
#include <catalog/pg_tablespace.h>
#include <catalog/pg_type.h>
#include <lib/ilist.h>
#include <mb/pg_wchar.h>
#include <miscadmin.h>
#include <storage/ipc.h>
#include <storage/lmgr.h>
#include <utils/builtins.h>
#include <utils/lsyscache.h>
#include <utils/selfuncs.h>
#include <utils/typcache.h>

#include <groonga.h>

#include <stdlib.h>

PG_MODULE_MAGIC;

static bool PGrnIsLZ4Available;
static relopt_kind PGrnReloptionKind;

typedef struct PGrnOptions
{
	int32 vl_len_;
	int tokenizerOffset;
	int normalizerOffset;
} PGrnOptions;

typedef struct PGrnCreateData
{
	Relation index;
	grn_obj *idsTable;
	grn_obj *lexicons;
	unsigned int i;
	TupleDesc desc;
	Oid relNode;
	bool forFullTextSearch;
	grn_id attributeTypeID;
} PGrnCreateData;

typedef struct PGrnBuildStateData
{
	grn_obj	*idsTable;
	double nIndexedTuples;
} PGrnBuildStateData;

typedef PGrnBuildStateData *PGrnBuildState;

typedef struct PGrnScanOpaqueData
{
	slist_node node;
	Oid dataTableID;
	grn_obj *idsTable;
	grn_obj minBorderValue;
	grn_obj maxBorderValue;
	grn_obj *searched;
	grn_obj *sorted;
	grn_obj *targetTable;
	grn_obj *indexCursor;
	grn_table_cursor *tableCursor;
	grn_obj *keyAccessor;
	grn_obj *scoreAccessor;
	grn_id currentID;
} PGrnScanOpaqueData;

typedef PGrnScanOpaqueData *PGrnScanOpaque;

typedef struct PGrnSearchData
{
	grn_obj targetColumns;
	grn_obj matchTargets;
	grn_obj sectionID;
	grn_obj *expression;
	grn_obj *expressionVariable;
	bool    isEmptyCondition;
} PGrnSearchData;

static slist_head PGrnScanOpaques = SLIST_STATIC_INIT(PGrnScanOpaques);

PG_FUNCTION_INFO_V1(pgroonga_score);
PG_FUNCTION_INFO_V1(pgroonga_table_name);
PG_FUNCTION_INFO_V1(pgroonga_command);

PG_FUNCTION_INFO_V1(pgroonga_contain_text);
PG_FUNCTION_INFO_V1(pgroonga_contain_bpchar);
PG_FUNCTION_INFO_V1(pgroonga_match);

PG_FUNCTION_INFO_V1(pgroonga_insert);
PG_FUNCTION_INFO_V1(pgroonga_beginscan);
PG_FUNCTION_INFO_V1(pgroonga_gettuple);
PG_FUNCTION_INFO_V1(pgroonga_getbitmap);
PG_FUNCTION_INFO_V1(pgroonga_rescan);
PG_FUNCTION_INFO_V1(pgroonga_endscan);
PG_FUNCTION_INFO_V1(pgroonga_build);
PG_FUNCTION_INFO_V1(pgroonga_buildempty);
PG_FUNCTION_INFO_V1(pgroonga_bulkdelete);
PG_FUNCTION_INFO_V1(pgroonga_vacuumcleanup);
PG_FUNCTION_INFO_V1(pgroonga_costestimate);
PG_FUNCTION_INFO_V1(pgroonga_options);

static grn_ctx grnContext;
static grn_ctx *ctx = &grnContext;
static grn_obj buffer;
static grn_obj headBuffer;
static grn_obj bodyBuffer;
static grn_obj footBuffer;
static grn_obj inspectBuffer;

static const char *
PGrnInspect(grn_obj *object)
{
	GRN_BULK_REWIND(&inspectBuffer);
	grn_inspect(ctx, &inspectBuffer, object);
	GRN_TEXT_PUTC(ctx, &inspectBuffer, '\0');
	return GRN_TEXT_VALUE(&inspectBuffer);
}

static grn_encoding
PGrnGetEncoding(void)
{
	int	enc = GetDatabaseEncoding();

	switch (enc)
	{
	case PG_SQL_ASCII:
	case PG_UTF8:
		return GRN_ENC_UTF8;
	case PG_EUC_JP:
	case PG_EUC_JIS_2004:
		return GRN_ENC_EUC_JP;
	case PG_LATIN1:
		return GRN_ENC_LATIN1;
	case PG_KOI8R:
		return GRN_ENC_KOI8R;
	case PG_SJIS:
	case PG_SHIFT_JIS_2004:
		return GRN_ENC_SJIS;
	default:
		elog(WARNING,
			 "pgroonga: use default encoding instead of '%s'",
			 GetDatabaseEncodingName());
		return GRN_ENC_DEFAULT;
	}
}

static void
PGrnEnsureDatabase(void)
{
	char path[MAXPGPATH];
	grn_obj	*db;

	GRN_CTX_SET_ENCODING(ctx, PGrnGetEncoding());
	join_path_components(path,
						 GetDatabasePath(MyDatabaseId, DEFAULTTABLESPACE_OID),
						 PGrnDatabaseBasename);

	db = grn_db_open(ctx, path);
	if (db)
		return;

	db = grn_db_create(ctx, path, NULL);
	if (!db)
		ereport(ERROR,
				(errcode(ERRCODE_IO_ERROR),
				 errmsg("pgroonga: failed to create database: <%s>: %s",
						path, ctx->errbuf)));
}

static void
PGrnOnProcExit(int code, Datum arg)
{
	grn_obj *db;

	GRN_OBJ_FIN(ctx, &inspectBuffer);
	GRN_OBJ_FIN(ctx, &footBuffer);
	GRN_OBJ_FIN(ctx, &bodyBuffer);
	GRN_OBJ_FIN(ctx, &headBuffer);
	GRN_OBJ_FIN(ctx, &buffer);

	db = grn_ctx_db(ctx);
	if (db)
		grn_obj_close(ctx, db);

	grn_ctx_fin(ctx);
	grn_fin();
}

static void
PGrnInitializeGroongaInformation(void)
{
	grn_obj grnIsSupported;

	GRN_BOOL_INIT(&grnIsSupported, 0);
	grn_obj_get_info(ctx, NULL, GRN_INFO_SUPPORT_LZ4, &grnIsSupported);
	PGrnIsLZ4Available = (GRN_BOOL_VALUE(&grnIsSupported));
	GRN_OBJ_FIN(ctx, &grnIsSupported);
}

static bool
PGrnIsTokenizer(grn_obj *object)
{
	if (object->header.type != GRN_PROC)
		return false;

  if (grn_proc_get_type(ctx, object) != GRN_PROC_TOKENIZER)
	  return false;

  return true;
}

static void
PGrnOptionValidateTokenizer(char *name)
{
	grn_obj *tokenizer;
	size_t name_length;

	name_length = strlen(name);
	if (name_length == 0)
		return;

	tokenizer = grn_ctx_get(ctx, name, name_length);
	if (!tokenizer)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("pgroonga: nonexistent tokenizer: <%s>",
						name)));
	}

	if (!PGrnIsTokenizer(tokenizer))
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("pgroonga: not tokenizer: <%s>: %s",
						name, PGrnInspect(tokenizer))));
	}
}

static bool
PGrnIsNormalizer(grn_obj *object)
{
	if (object->header.type != GRN_PROC)
		return false;

  if (grn_proc_get_type(ctx, object) != GRN_PROC_NORMALIZER)
	  return false;

  return true;
}

static void
PGrnOptionValidateNormalizer(char *name)
{
	grn_obj *normalizer;
	size_t name_length;

	name_length = strlen(name);
	if (name_length == 0)
		return;

	normalizer = grn_ctx_get(ctx, name, name_length);
	if (!normalizer)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("pgroonga: nonexistent normalizer: <%s>",
						name)));
	}

	if (!PGrnIsNormalizer(normalizer))
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("pgroonga: not normalizer: <%s>: %s",
						name, PGrnInspect(normalizer))));
	}
}

static void
PGrnInitializeOptions(void)
{
	PGrnReloptionKind = add_reloption_kind();

	add_string_reloption(PGrnReloptionKind,
						 "tokenizer",
						 "Tokenizer name to be used for full-text search",
						 PGRN_DEFAULT_TOKENIZER,
						 PGrnOptionValidateTokenizer);
	add_string_reloption(PGrnReloptionKind,
						 "normalizer",
						 "Normalizer name to be used for full-text search",
						 PGRN_DEFAULT_NORMALIZER,
						 PGrnOptionValidateNormalizer);
}

void
_PG_init(void)
{
	if (grn_init() != GRN_SUCCESS)
		ereport(ERROR,
				(errcode(ERRCODE_SYSTEM_ERROR),
				 errmsg("pgroonga: failed to initialize Groonga")));
	if (grn_ctx_init(ctx, 0))
		ereport(ERROR,
				(errcode(ERRCODE_SYSTEM_ERROR),
				 errmsg("pgroonga: failed to initialize Groonga context")));

	on_proc_exit(PGrnOnProcExit, 0);

	GRN_VOID_INIT(&buffer);
	GRN_TEXT_INIT(&headBuffer, 0);
	GRN_TEXT_INIT(&bodyBuffer, 0);
	GRN_TEXT_INIT(&footBuffer, 0);
	GRN_TEXT_INIT(&inspectBuffer, 0);

	PGrnEnsureDatabase();

	PGrnInitializeGroongaInformation();

	PGrnInitializeOptions();
}

static int
PGrnRCToPgErrorCode(grn_rc rc)
{
	int errorCode = ERRCODE_SYSTEM_ERROR;

	/* TODO: Fill me. */
	switch (rc)
	{
	case GRN_NO_SUCH_FILE_OR_DIRECTORY:
		errorCode = ERRCODE_IO_ERROR;
		break;
	case GRN_INPUT_OUTPUT_ERROR:
		errorCode = ERRCODE_IO_ERROR;
		break;
	case GRN_INVALID_ARGUMENT:
		errorCode = ERRCODE_INVALID_PARAMETER_VALUE;
		break;
	default:
		break;
	}

	return errorCode;
}

static grn_bool
PGrnCheck(const char *message)
{
	if (ctx->rc == GRN_SUCCESS)
		return GRN_TRUE;

	ereport(ERROR,
			(errcode(PGrnRCToPgErrorCode(ctx->rc)),
			 errmsg("pgroonga: %s: %s", message, ctx->errbuf)));
	return GRN_FALSE;
}

static grn_id
PGrnGetType(Relation index, AttrNumber n)
{
	TupleDesc desc = RelationGetDescr(index);
	Form_pg_attribute attr;
	grn_id typeID = GRN_ID_NIL;
	int32 maxlen;

	attr = desc->attrs[n];

	/* TODO: support array and record types. */
	switch (attr->atttypid)
	{
	case BOOLOID:
		typeID = GRN_DB_BOOL;
		break;
	case INT2OID:
		typeID = GRN_DB_INT16;
		break;
	case INT4OID:
		typeID = GRN_DB_INT32;
		break;
	case INT8OID:
		typeID = GRN_DB_INT64;
		break;
	case FLOAT4OID:
	case FLOAT8OID:
		typeID = GRN_DB_FLOAT;
		break;
	case TIMESTAMPOID:
	case TIMESTAMPTZOID:
#ifdef HAVE_INT64_TIMESTAMP
		typeID = GRN_DB_INT64;	/* FIXME: use GRN_DB_TIME instead */
#else
		typeID = GRN_DB_FLOAT;
#endif
		break;
	case TEXTOID:
	case XMLOID:
		typeID = GRN_DB_LONG_TEXT;
		break;
	case BPCHAROID:
	case VARCHAROID:
		maxlen = type_maximum_size(attr->atttypid, attr->atttypmod);
		if (maxlen >= 0)
		{
			if (maxlen < 4096)
				typeID = GRN_DB_SHORT_TEXT;	/* 4KB */
			if (maxlen < 64 * 1024)
				typeID = GRN_DB_TEXT;			/* 64KB */
		}
		else
		{
			typeID = GRN_DB_LONG_TEXT;
		}
		break;
#ifdef NOT_USED
	case POINTOID:
		typeID = GRN_DB_TOKYO_GEO_POINT or GRN_DB_WGS84_GEO_POINT;
		break;
#endif
	default:
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("pgroonga: unsupported type: %u", attr->atttypid)));
		break;
	}

	return typeID;
}

static void
PGrnGetValue(Relation index, AttrNumber n, grn_obj *buffer, Datum value)
{
	FmgrInfo *function;

	function = index_getprocinfo(index, n + 1, PGrnGetValueProc);
	FunctionCall3(function,
				  PointerGetDatum(ctx), PointerGetDatum(buffer),
				  value);
}

static grn_obj *
PGrnLookup(const char *name, int errorLevel)
{
	grn_obj *object = grn_ctx_get(ctx, name, strlen(name));
	if (!object)
		ereport(errorLevel,
				(errcode(ERRCODE_INVALID_NAME),
				 errmsg("pgroonga: object isn't found: <%s>", name)));
	return object;
}

static grn_obj *
PGrnLookupIDsTable(Relation index, int errorLevel)
{
	char name[GRN_TABLE_MAX_KEY_SIZE];

	snprintf(name, sizeof(name), PGrnIDsTableNameFormat, index->rd_node.relNode);
	return PGrnLookup(name, errorLevel);
}

static grn_obj *
PGrnLookupIndexColumn(Relation index, unsigned int nthAttribute, int errorLevel)
{
	char name[GRN_TABLE_MAX_KEY_SIZE];

	snprintf(name, sizeof(name),
			 PGrnLexiconNameFormat ".%s",
			 index->rd_node.relNode,
			 nthAttribute,
			 PGrnIndexColumnName);
	return PGrnLookup(name, errorLevel);
}

static grn_obj *
PGrnCreateTable(const char *name,
				grn_obj_flags flags,
				grn_obj *type)
{
	grn_obj	*table;

	table = grn_table_create(ctx,
							 name, strlen(name), NULL,
							 GRN_OBJ_PERSISTENT | flags,
							 type,
							 NULL);
	PGrnCheck("pgroonga: failed to create table");

	return table;
}

static grn_obj *
PGrnCreateColumn(grn_obj	*table,
				 const char *name,
				 grn_obj_flags flags,
				 grn_obj	*type)
{
	grn_obj *column;

    column = grn_column_create(ctx, table,
							   name, strlen(name), NULL,
							   GRN_OBJ_PERSISTENT | flags,
							   type);
	PGrnCheck("pgroonga: failed to create column");

	return column;
}

static bool
PGrnIsForFullTextSearchIndex(Relation index)
{
	Oid containStrategyOID;
	containStrategyOID = get_opfamily_member(index->rd_opfamily[0],
											 index->rd_opcintype[0],
											 index->rd_opcintype[0],
											 PGrnContainStrategyNumber);
	return (containStrategyOID != InvalidOid);
}

static void
PGrnCreateDataColumn(PGrnCreateData *data)
{
	grn_obj_flags flags = GRN_OBJ_COLUMN_SCALAR;

	if (PGrnIsLZ4Available)
	{
		switch (data->attributeTypeID)
		{
		case GRN_DB_SHORT_TEXT:
		case GRN_DB_TEXT:
		case GRN_DB_LONG_TEXT:
			flags |= GRN_OBJ_COMPRESS_LZ4;
			break;
		}
	}

	PGrnCreateColumn(data->idsTable,
					 data->desc->attrs[data->i]->attname.data,
					 flags,
					 grn_ctx_at(ctx, data->attributeTypeID));
}

static void
PGrnCreateIndexColumn(PGrnCreateData *data)
{
	grn_id typeID = GRN_ID_NIL;
	char lexiconName[GRN_TABLE_MAX_KEY_SIZE];
	grn_obj *lexicon;

	switch (data->attributeTypeID)
	{
	case GRN_DB_TEXT:
	case GRN_DB_LONG_TEXT:
		typeID = GRN_DB_SHORT_TEXT;
		break;
	default:
		typeID = data->attributeTypeID;
		break;
	}

	snprintf(lexiconName, sizeof(lexiconName),
			 PGrnLexiconNameFormat, data->relNode, data->i);
	lexicon = PGrnCreateTable(lexiconName,
							  GRN_OBJ_TABLE_PAT_KEY,
							  grn_ctx_at(ctx, typeID));
	GRN_PTR_PUT(ctx, data->lexicons, lexicon);

	if (data->forFullTextSearch)
	{
		PGrnOptions *options;
		const char *tokenizerName = PGRN_DEFAULT_TOKENIZER;
		const char *normalizerName = PGRN_DEFAULT_NORMALIZER;

		options = (PGrnOptions *) (data->index->rd_options);
		if (options)
		{
			tokenizerName = (const char *) (options) + options->tokenizerOffset;
			normalizerName = (const char *) (options) + options->normalizerOffset;
		}
		if (tokenizerName && tokenizerName[0])
		{
			grn_obj_set_info(ctx, lexicon, GRN_INFO_DEFAULT_TOKENIZER,
							 PGrnLookup(tokenizerName, ERROR));
		}
		if (normalizerName && normalizerName[0])
		{
			grn_obj_set_info(ctx, lexicon, GRN_INFO_NORMALIZER,
							 PGrnLookup(normalizerName, ERROR));
		}
	}

	{
		grn_obj_flags flags = GRN_OBJ_COLUMN_INDEX;
		if (data->forFullTextSearch)
			flags |= GRN_OBJ_WITH_POSITION;
		PGrnCreateColumn(lexicon,
						 PGrnIndexColumnName,
						 flags,
						 data->idsTable);
	}
}

/**
 * PGrnCreate
 *
 * @param	ctx
 * @param	index
 */
static void
PGrnCreate(Relation index, grn_obj **idsTable, grn_obj *lexicons)
{
	char idsTableName[GRN_TABLE_MAX_KEY_SIZE];
	PGrnCreateData data;

	data.index = index;
	data.desc = RelationGetDescr(index);
	data.relNode = index->rd_node.relNode;

	snprintf(idsTableName, sizeof(idsTableName),
			 PGrnIDsTableNameFormat, data.relNode);
	*idsTable = PGrnCreateTable(idsTableName,
								GRN_OBJ_TABLE_PAT_KEY,
								grn_ctx_at(ctx, GRN_DB_UINT64));
	data.idsTable = *idsTable;
	data.lexicons = lexicons;
	data.forFullTextSearch = PGrnIsForFullTextSearchIndex(index);

	for (data.i = 0; data.i < data.desc->natts; data.i++)
	{
		data.attributeTypeID = PGrnGetType(index, data.i);
		PGrnCreateDataColumn(&data);
		PGrnCreateIndexColumn(&data);
	}
}

static void
PGrnSetSources(Relation index, grn_obj *idsTable)
{
	TupleDesc desc;
	grn_obj sourceIDs;
	int i;

	desc = RelationGetDescr(index);
	GRN_RECORD_INIT(&sourceIDs, GRN_OBJ_VECTOR, GRN_ID_NIL);
	for (i = 0; i < desc->natts; i++)
	{
		NameData *name = &(desc->attrs[i]->attname);
		grn_obj *source;
		grn_id sourceID;
		grn_obj *indexColumn;

		GRN_BULK_REWIND(&sourceIDs);

		source = grn_obj_column(ctx, idsTable, name->data, strlen(name->data));
		sourceID = grn_obj_id(ctx, source);
		grn_obj_unlink(ctx, source);
		GRN_RECORD_PUT(ctx, &sourceIDs, sourceID);

		indexColumn = PGrnLookupIndexColumn(index, i, ERROR);
		grn_obj_set_info(ctx, indexColumn, GRN_INFO_SOURCE, &sourceIDs);
	}
	GRN_OBJ_FIN(ctx, &sourceIDs);
}

static grn_id
CtidToUInt64(ItemPointer ctid)
{
	BlockNumber blockNumber;
	OffsetNumber offsetNumber;

	blockNumber = ItemPointerGetBlockNumber(ctid);
	offsetNumber = ItemPointerGetOffsetNumber(ctid);
	return (blockNumber << 16 | offsetNumber);
}

static ItemPointerData
UInt64ToCtid(uint64 key)
{
	ItemPointerData	ctid;
	ItemPointerSet(&ctid, (key >> 16) & 0xFFFFFFFF, key & 0xFFFF);
	return ctid;
}

static double
PGrnCollectScore(Oid tableID, ItemPointer ctid)
{
	double score = 0.0;
	uint64_t key;
	grn_id recordID = GRN_ID_NIL;
	slist_iter iter;
	grn_obj scoreBuffer;

	key = CtidToUInt64(ctid);
	GRN_FLOAT_INIT(&scoreBuffer, 0);

	slist_foreach(iter, &PGrnScanOpaques)
	{
		PGrnScanOpaque so;
		grn_id id;

		so = slist_container(PGrnScanOpaqueData, node, iter.cur);
		if (so->dataTableID != tableID)
			continue;

		if (!so->scoreAccessor)
			continue;

		if (recordID == GRN_ID_NIL)
		{
			recordID = grn_table_get(ctx, so->idsTable, &key, sizeof(uint64_t));
		}

		id = grn_table_get(ctx, so->searched, &recordID, sizeof(grn_id));
		if (id == GRN_ID_NIL)
			continue;

		GRN_BULK_REWIND(&scoreBuffer);
		grn_obj_get_value(ctx, so->scoreAccessor, id, &scoreBuffer);
		if (scoreBuffer.header.domain == GRN_DB_FLOAT)
		{
			score += GRN_FLOAT_VALUE(&scoreBuffer);
		}
		else
		{
			score += GRN_INT32_VALUE(&scoreBuffer);
		}
	}

	GRN_OBJ_FIN(ctx, &scoreBuffer);

	return score;
}

/**
 * pgroonga.score(row record) : float8
 */
Datum
pgroonga_score(PG_FUNCTION_ARGS)
{
	HeapTupleHeader header = PG_GETARG_HEAPTUPLEHEADER(0);
	Oid	type;
	int32 recordType;
	TupleDesc desc;
	double score = 0.0;

	type = HeapTupleHeaderGetTypeId(header);
	recordType = HeapTupleHeaderGetTypMod(header);
	desc = lookup_rowtype_tupdesc(type, recordType);

	if (desc->natts > 0)
	{
		Oid tableID = desc->attrs[0]->attrelid;
		score = PGrnCollectScore(tableID, &(header->t_ctid));
	}

	ReleaseTupleDesc(desc);

	PG_RETURN_FLOAT8(score);
}

/**
 * pgroonga.table_name(indexName cstring) : cstring
 */
Datum
pgroonga_table_name(PG_FUNCTION_ARGS)
{
	Datum indexNameDatum = PG_GETARG_DATUM(0);
	Datum indexOidDatum;
	Oid indexOid;
	char tableName[GRN_TABLE_MAX_KEY_SIZE];
	char *copiedTableName;

	indexOidDatum = DirectFunctionCall1(to_regclass, indexNameDatum);
	if (!indexOidDatum)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_NAME),
				 errmsg("unknown index name: <%s>",
						DatumGetCString(indexNameDatum))));
	}
	indexOid = DatumGetObjectId(indexOidDatum);

	snprintf(tableName, sizeof(tableName), PGrnIDsTableNameFormat, indexOid);
	copiedTableName = pstrdup(tableName);
	PG_RETURN_CSTRING(copiedTableName);
}

/**
 * pgroonga.command(groongaCommand text) : text
 */
Datum
pgroonga_command(PG_FUNCTION_ARGS)
{
	text *groongaCommand = PG_GETARG_TEXT_PP(0);
	grn_rc rc;
	int flags = 0;
	text *result;

	grn_ctx_send(ctx,
				 VARDATA_ANY(groongaCommand),
				 VARSIZE_ANY_EXHDR(groongaCommand), 0);
	rc = ctx->rc;

	GRN_BULK_REWIND(&bodyBuffer);
	do {
		char *chunk;
		unsigned int chunkSize;
		grn_ctx_recv(ctx, &chunk, &chunkSize, &flags);
		GRN_TEXT_PUT(ctx, &bodyBuffer, chunk, chunkSize);
	} while ((flags & GRN_CTX_MORE));

	GRN_BULK_REWIND(&headBuffer);
	GRN_BULK_REWIND(&footBuffer);
	grn_output_envelope(ctx,
						rc,
						&headBuffer,
						&bodyBuffer,
						&footBuffer,
						NULL,
						0);

	grn_obj_reinit(ctx, &buffer, GRN_DB_TEXT, 0);
	GRN_TEXT_PUT(ctx, &buffer,
				 GRN_TEXT_VALUE(&headBuffer), GRN_TEXT_LEN(&headBuffer));
	GRN_TEXT_PUT(ctx, &buffer,
				 GRN_TEXT_VALUE(&bodyBuffer), GRN_TEXT_LEN(&bodyBuffer));
	GRN_TEXT_PUT(ctx, &buffer,
				 GRN_TEXT_VALUE(&footBuffer), GRN_TEXT_LEN(&footBuffer));
	result = cstring_to_text_with_len(GRN_TEXT_VALUE(&buffer),
									  GRN_TEXT_LEN(&buffer));
	PG_RETURN_TEXT_P(result);
}

static grn_bool
pgroonga_contain_raw(const char *text, unsigned int text_size,
					 const char *key, unsigned int key_size)
{
	grn_bool contained = GRN_FALSE;
	grn_obj buffer;
	grn_obj *expression, *expressionVariable;

	GRN_EXPR_CREATE_FOR_QUERY(ctx, NULL, expression, expressionVariable);

	GRN_TEXT_INIT(&buffer, 0);

	GRN_TEXT_SET(ctx, &buffer, text, text_size);
	grn_expr_append_const(ctx, expression, &buffer, GRN_OP_PUSH, 1);

	GRN_TEXT_SET(ctx, &buffer, key, key_size);
	grn_expr_append_const(ctx, expression, &buffer, GRN_OP_PUSH, 1);

	grn_expr_append_op(ctx, expression, GRN_OP_MATCH, 2);

	{
		grn_obj *result;
		result = grn_expr_exec(ctx, expression, 0);
		if (ctx->rc)
		{
			goto exit;
		}
		contained = GRN_INT32_VALUE(result) != 0;
	}

exit:
	grn_obj_unlink(ctx, expression);
	GRN_OBJ_FIN(ctx, &buffer);

	return contained;
}

/**
 * pgroonga.contain(doc text, key text) : bool
 */
Datum
pgroonga_contain_text(PG_FUNCTION_ARGS)
{
	text *doc = PG_GETARG_TEXT_PP(0);
	text *key = PG_GETARG_TEXT_PP(1);
	grn_bool contained;

	contained = pgroonga_contain_raw(VARDATA_ANY(doc), VARSIZE_ANY_EXHDR(doc),
									 VARDATA_ANY(key), VARSIZE_ANY_EXHDR(key));
	PG_RETURN_BOOL(contained);
}

/**
 * pgroonga.contain(doc bpchar, key bpchar) : bool
 */
Datum
pgroonga_contain_bpchar(PG_FUNCTION_ARGS)
{
	BpChar *doc = PG_GETARG_BPCHAR_PP(0);
	BpChar *key = PG_GETARG_BPCHAR_PP(1);
	grn_bool contained;

	contained =
		pgroonga_contain_raw(VARDATA_ANY(doc), pgroonga_bpchar_size(doc),
							 VARDATA_ANY(key), pgroonga_bpchar_size(key));
	PG_RETURN_BOOL(contained);
}

/**
 * pgroonga.match(text, query) : bool
 */
Datum
pgroonga_match(PG_FUNCTION_ARGS)
{
#ifdef NOT_USED
	text *text = PG_GETARG_TEXT_PP(0);
	text *query = PG_GETARG_TEXT_PP(1);
#endif

	ereport(ERROR,
			(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
			 errmsg("pgroonga: operator @@ is available only in index scans")));

	PG_RETURN_BOOL(false);
}

static void
PGrnInsert(grn_ctx *ctx,
		  Relation index,
		  grn_obj *idsTable,
		  Datum *values,
		  bool *isnull,
		  ItemPointer ht_ctid)
{
	TupleDesc desc = RelationGetDescr(index);
	uint64 key = CtidToUInt64(ht_ctid);
	grn_id id;
	int i;

	id = grn_table_add(ctx, idsTable, &key, sizeof(uint64), NULL);

	for (i = 0; i < desc->natts; i++)
	{
		grn_obj *dataColumn;
		NameData *name = &(desc->attrs[i]->attname);

		if (isnull[i])
			continue;

		dataColumn = grn_obj_column(ctx, idsTable,
									name->data, strlen(name->data));
		grn_obj_reinit(ctx, &buffer, PGrnGetType(index, i), 0);
		PGrnGetValue(index, i, &buffer, values[i]);
		grn_obj_set_value(ctx, dataColumn, id, &buffer, GRN_OBJ_SET);
		grn_obj_unlink(ctx, dataColumn);
		if (!PGrnCheck("pgroonga: failed to set column value")) {
			continue;
		}
	}
}

/**
 * pgroonga.insert() -- aminsert
 */
Datum
pgroonga_insert(PG_FUNCTION_ARGS)
{
	Relation index = (Relation) PG_GETARG_POINTER(0);
	Datum *values = (Datum *) PG_GETARG_POINTER(1);
	bool *isnull = (bool *) PG_GETARG_POINTER(2);
	ItemPointer ht_ctid = (ItemPointer) PG_GETARG_POINTER(3);
#ifdef NOT_USED
	Relation heap = (Relation) PG_GETARG_POINTER(4);
	IndexUniqueCheck checkUnique = PG_GETARG_INT32(5);
#endif
	grn_obj *idsTable = PGrnLookupIDsTable(index, ERROR);

	PGrnInsert(ctx, index, idsTable, values, isnull, ht_ctid);

	PG_RETURN_BOOL(true);
}

static void
PGrnScanOpaqueInit(PGrnScanOpaque so, Relation index)
{
	so->dataTableID = index->rd_index->indrelid;
	so->idsTable = PGrnLookupIDsTable(index, ERROR);
	GRN_VOID_INIT(&(so->minBorderValue));
	GRN_VOID_INIT(&(so->maxBorderValue));
	so->searched = NULL;
	so->sorted = NULL;
	so->targetTable = NULL;
	so->indexCursor = NULL;
	so->tableCursor = NULL;
	so->keyAccessor = NULL;
	so->scoreAccessor = NULL;
	so->currentID = GRN_ID_NIL;

	slist_push_head(&PGrnScanOpaques, &(so->node));
}

static void
PGrnScanOpaqueReinit(PGrnScanOpaque so)
{
	so->currentID = GRN_ID_NIL;
	if (so->scoreAccessor)
	{
		grn_obj_unlink(ctx, so->scoreAccessor);
		so->scoreAccessor = NULL;
	}
	if (so->keyAccessor)
	{
		grn_obj_unlink(ctx, so->keyAccessor);
		so->keyAccessor = NULL;
	}
	if (so->indexCursor)
	{
		grn_obj_close(ctx, so->indexCursor);
		so->indexCursor = NULL;
	}
	if (so->tableCursor)
	{
		grn_table_cursor_close(ctx, so->tableCursor);
		so->tableCursor = NULL;
	}
	GRN_OBJ_FIN(ctx, &(so->minBorderValue));
	GRN_OBJ_FIN(ctx, &(so->maxBorderValue));
	if (so->sorted)
	{
		grn_obj_unlink(ctx, so->sorted);
		so->sorted = NULL;
	}
	if (so->searched)
	{
		grn_obj_unlink(ctx, so->searched);
		so->searched = NULL;
	}
}

static void
PGrnScanOpaqueFin(PGrnScanOpaque so)
{
	slist_mutable_iter iter;

	slist_foreach_modify(iter, &PGrnScanOpaques)
	{
		PGrnScanOpaque currentSo;

		currentSo = slist_container(PGrnScanOpaqueData, node, iter.cur);
		if (currentSo == so)
		{
			slist_delete_current(&iter);
			break;
		}
	}

	PGrnScanOpaqueReinit(so);
}

/**
 * pgroonga.beginscan() -- ambeginscan
 */
Datum
pgroonga_beginscan(PG_FUNCTION_ARGS)
{
	Relation index = (Relation) PG_GETARG_POINTER(0);
	int nkeys = PG_GETARG_INT32(1);
	int norderbys = PG_GETARG_INT32(2);
	IndexScanDesc scan;
	PGrnScanOpaque so;

	scan = RelationGetIndexScan(index, nkeys, norderbys);

	so = (PGrnScanOpaque) palloc(sizeof(PGrnScanOpaqueData));
	PGrnScanOpaqueInit(so, index);

	scan->opaque = so;

	PG_RETURN_POINTER(scan);
}

static void
PGrnSearchBuildConditionLike(PGrnSearchData *data,
							 grn_obj *matchTarget,
							 grn_obj *query)
{
	grn_obj *expression;
	const char *queryRaw;
	size_t querySize;

	expression = data->expression;
	queryRaw = GRN_TEXT_VALUE(query);
	querySize = GRN_TEXT_LEN(query);

	if (querySize == 0)
	{
		data->isEmptyCondition = true;
		return;
	}

	if (!(queryRaw[0] == '%' && queryRaw[querySize - 1] == '%'))
	{
		data->isEmptyCondition = true;
		return;
	}

	grn_expr_append_obj(ctx, expression, matchTarget, GRN_OP_PUSH, 1);
	grn_expr_append_const_str(ctx, expression,
							  queryRaw + 1, querySize - 2,
							  GRN_OP_PUSH, 1);
	grn_expr_append_op(ctx, expression, GRN_OP_MATCH, 2);
}

static void
PGrnSearchBuildConditions(IndexScanDesc scan,
						  PGrnScanOpaque so,
						  PGrnSearchData *data)
{
	Relation index = scan->indexRelation;
	TupleDesc desc;
	int i, nExpressions = 0;

	desc = RelationGetDescr(index);
	for (i = 0; i < scan->numberOfKeys; i++)
	{
		ScanKey key = &(scan->keyData[i]);
		grn_bool isValidStrategy = GRN_TRUE;
		const char *targetColumnName;
		grn_obj *targetColumn;
		grn_obj *matchTarget, *matchTargetVariable;
		grn_operator operator = GRN_OP_NOP;

		/* NULL key is not supported */
		if (key->sk_flags & SK_ISNULL)
			continue;

		GRN_EXPR_CREATE_FOR_QUERY(ctx, so->idsTable,
								  matchTarget, matchTargetVariable);
		GRN_PTR_PUT(ctx, &(data->matchTargets), matchTarget);

		targetColumnName = desc->attrs[key->sk_attno - 1]->attname.data;
		targetColumn = grn_obj_column(ctx, so->idsTable,
									  targetColumnName,
									  strlen(targetColumnName));
		GRN_PTR_PUT(ctx, &(data->targetColumns), targetColumn);
		grn_expr_append_obj(ctx, matchTarget, targetColumn, GRN_OP_PUSH, 1);

		GRN_UINT32_SET(ctx, &(data->sectionID), key->sk_attno - 1);
		grn_expr_append_const(ctx, matchTarget,
							  &(data->sectionID), GRN_OP_PUSH, 1);

		grn_expr_append_op(ctx, matchTarget, GRN_OP_GET_MEMBER, 2);

		grn_obj_reinit(ctx, &buffer, PGrnGetType(index, key->sk_attno - 1), 0);
		PGrnGetValue(index, key->sk_attno - 1, &buffer, key->sk_argument);

		switch (key->sk_strategy)
		{
		case PGrnLessStrategyNumber:
			operator = GRN_OP_LESS;
			break;
		case PGrnLessEqualStrategyNumber:
			operator = GRN_OP_LESS_EQUAL;
			break;
		case PGrnEqualStrategyNumber:
			operator = GRN_OP_EQUAL;
			break;
		case PGrnGreaterEqualStrategyNumber:
			operator = GRN_OP_GREATER_EQUAL;
			break;
		case PGrnGreaterStrategyNumber:
			operator = GRN_OP_GREATER;
			break;
		case PGrnLikeStrategyNumber:
			break;
		case PGrnContainStrategyNumber:
			operator = GRN_OP_MATCH;
			break;
		case PGrnQueryStrategyNumber:
			break;
		default:
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("unexpected strategy number: %d",
							key->sk_strategy)));
			isValidStrategy = GRN_FALSE;
			break;
		}

		if (!isValidStrategy)
			continue;

		switch (key->sk_strategy)
		{
		case PGrnLikeStrategyNumber:
			PGrnSearchBuildConditionLike(data, matchTarget, &buffer);
			if (data->isEmptyCondition)
				return;
			break;
		case PGrnQueryStrategyNumber:
		{
			grn_rc rc;
			grn_expr_flags flags =
				GRN_EXPR_SYNTAX_QUERY | GRN_EXPR_ALLOW_LEADING_NOT;
			rc = grn_expr_parse(ctx, data->expression,
								GRN_TEXT_VALUE(&buffer), GRN_TEXT_LEN(&buffer),
								matchTarget, GRN_OP_MATCH, GRN_OP_AND,
								flags);
			if (rc != GRN_SUCCESS)
			{
				ereport(ERROR,
						(errcode(PGrnRCToPgErrorCode(rc)),
						 errmsg("pgroonga: failed to parse expression: %s",
								ctx->errbuf)));
			}
			break;
		}
		default:
			grn_expr_append_obj(ctx, data->expression,
								matchTarget, GRN_OP_PUSH, 1);
			grn_expr_append_const(ctx, data->expression,
								  &buffer, GRN_OP_PUSH, 1);
			grn_expr_append_op(ctx, data->expression, operator, 2);
			break;
		}

		if (nExpressions > 0)
			grn_expr_append_op(ctx, data->expression, GRN_OP_AND, 2);
		nExpressions++;
	}
}

static void
PGrnSearchDataFree(PGrnSearchData *data)
{
	unsigned int i;
	unsigned int nMatchTargets;
	unsigned int nTargetColumns;

	grn_obj_unlink(ctx, data->expression);

	nMatchTargets = GRN_BULK_VSIZE(&(data->matchTargets)) / sizeof(grn_obj *);
	for (i = 0; i < nMatchTargets; i++)
	{
		grn_obj *matchTarget = GRN_PTR_VALUE_AT(&(data->matchTargets), i);
		grn_obj_unlink(ctx, matchTarget);
	}
	GRN_OBJ_FIN(ctx, &(data->matchTargets));

	nTargetColumns = GRN_BULK_VSIZE(&(data->targetColumns)) / sizeof(grn_obj *);
	for (i = 0; i < nTargetColumns; i++)
	{
		grn_obj *targetColumn = GRN_PTR_VALUE_AT(&(data->targetColumns), i);
		grn_obj_unlink(ctx, targetColumn);
	}
	GRN_OBJ_FIN(ctx, &(data->targetColumns));

	GRN_OBJ_FIN(ctx, &(data->sectionID));
}

static void
PGrnSearch(IndexScanDesc scan)
{
	PGrnScanOpaque so = (PGrnScanOpaque) scan->opaque;
	PGrnSearchData data;

	if (scan->numberOfKeys == 0)
		return;

	GRN_PTR_INIT(&(data.matchTargets), GRN_OBJ_VECTOR, GRN_ID_NIL);
	GRN_PTR_INIT(&(data.targetColumns), GRN_OBJ_VECTOR, GRN_ID_NIL);
	GRN_UINT32_INIT(&(data.sectionID), 0);

	GRN_EXPR_CREATE_FOR_QUERY(ctx, so->idsTable,
							  data.expression, data.expressionVariable);
	data.isEmptyCondition = false;

	PG_TRY();
	{
		PGrnSearchBuildConditions(scan, so, &data);
	}
	PG_CATCH();
	{
		PGrnSearchDataFree(&data);
		PG_RE_THROW();
	}
	PG_END_TRY();

	/* TODO: Add NULL check for so->searched. */
	so->searched = grn_table_create(ctx, NULL, 0, NULL,
									GRN_OBJ_TABLE_HASH_KEY | GRN_OBJ_WITH_SUBREC,
									so->idsTable, 0);
	if (!data.isEmptyCondition)
	{
		grn_table_select(ctx,
						 so->idsTable,
						 data.expression,
						 so->searched,
						 GRN_OP_OR);
	}
	PGrnSearchDataFree(&data);
}

static void
PGrnSort(IndexScanDesc scan)
{
	/* TODO */
}

static void
PGrnOpenTableCursor(IndexScanDesc scan, ScanDirection dir)
{
	PGrnScanOpaque so = (PGrnScanOpaque) scan->opaque;
	grn_obj *table;
	int offset = 0;
	int limit = -1;
	int flags = 0;

	table = so->sorted;
	if (!table)
		table = so->searched;
	if (!table)
		table = so->idsTable;

	if (dir == BackwardScanDirection)
		flags |= GRN_CURSOR_DESCENDING;
	else
		flags |= GRN_CURSOR_ASCENDING;

	so->tableCursor = grn_table_cursor_open(ctx, table,
											NULL, 0, NULL, 0,
											offset, limit, flags);
	so->keyAccessor = grn_obj_column(ctx, table,
									 GRN_COLUMN_NAME_KEY,
									 GRN_COLUMN_NAME_KEY_LEN);
	if (so->searched)
	{
		so->scoreAccessor = grn_obj_column(ctx, so->searched,
										   GRN_COLUMN_NAME_SCORE,
										   GRN_COLUMN_NAME_SCORE_LEN);
	}
}

static bool
PGrnIsMeaningfullMaxBorderValue(grn_obj *currentValue,
								grn_obj *newValue,
								int flags,
								StrategyNumber strategy)
{
	if (((flags & GRN_CURSOR_LT) == GRN_CURSOR_LT) &&
		strategy == PGrnLessEqualStrategyNumber)
	{
		return grn_operator_exec_greater_equal(ctx, currentValue, newValue);
	}
	else
	{
		return grn_operator_exec_greater(ctx, currentValue, newValue);
	}
}

static bool
PGrnIsMeaningfullMinBorderValue(grn_obj *currentValue,
								grn_obj *newValue,
								int flags,
								StrategyNumber strategy)
{
	if (((flags & GRN_CURSOR_GT) == GRN_CURSOR_GT) &&
		strategy == PGrnGreaterEqualStrategyNumber)
	{
		return grn_operator_exec_less_equal(ctx, currentValue, newValue);
	}
	else
	{
		return grn_operator_exec_less(ctx, currentValue, newValue);
	}
}

static void
PGrnFillBorder(IndexScanDesc scan,
			   void **min, unsigned int *minSize,
			   void **max, unsigned int *maxSize,
			   int *flags)
{
	Relation index = scan->indexRelation;
	PGrnScanOpaque so = (PGrnScanOpaque) scan->opaque;
	grn_obj *minBorderValue;
	grn_obj *maxBorderValue;
	int i;

	minBorderValue = &(so->minBorderValue);
	maxBorderValue = &(so->maxBorderValue);
	for (i = 0; i < scan->numberOfKeys; i++)
	{
		ScanKey key = &(scan->keyData[i]);
		AttrNumber attrNumber;
		grn_id domain;

		attrNumber = key->sk_attno - 1;
		domain = PGrnGetType(index, attrNumber);
		switch (key->sk_strategy)
		{
		case PGrnLessStrategyNumber:
		case PGrnLessEqualStrategyNumber:
			if (maxBorderValue->header.type != GRN_DB_VOID)
			{
				grn_obj_reinit(ctx, &buffer, domain, 0);
				PGrnGetValue(index, attrNumber, &buffer, key->sk_argument);
				if (!PGrnIsMeaningfullMaxBorderValue(maxBorderValue,
													 &buffer,
													 *flags,
													 key->sk_strategy))
				{
					continue;
				}
			}
			grn_obj_reinit(ctx, maxBorderValue, domain, 0);
			PGrnGetValue(index, attrNumber, maxBorderValue, key->sk_argument);
			*max = GRN_BULK_HEAD(maxBorderValue);
			*maxSize = GRN_BULK_VSIZE(maxBorderValue);
			*flags &= ~(GRN_CURSOR_LT | GRN_CURSOR_LE);
			if (key->sk_strategy == PGrnLessStrategyNumber)
			{
				*flags |= GRN_CURSOR_LT;
			}
			else
			{
				*flags |= GRN_CURSOR_LE;
			}
			break;
		case PGrnGreaterEqualStrategyNumber:
		case PGrnGreaterStrategyNumber:
			if (minBorderValue->header.type != GRN_DB_VOID)
			{
				grn_obj_reinit(ctx, &buffer, domain, 0);
				PGrnGetValue(index, attrNumber, &buffer,
							 key->sk_argument);
				if (!PGrnIsMeaningfullMinBorderValue(minBorderValue,
													 &buffer,
													 *flags,
													 key->sk_strategy))
				{
					continue;
				}
			}
			grn_obj_reinit(ctx, minBorderValue, domain, 0);
			PGrnGetValue(index, attrNumber, minBorderValue, key->sk_argument);
			*min = GRN_BULK_HEAD(minBorderValue);
			*minSize = GRN_BULK_VSIZE(minBorderValue);
			*flags &= ~(GRN_CURSOR_GT | GRN_CURSOR_GE);
			if (key->sk_strategy == PGrnGreaterEqualStrategyNumber)
			{
				*flags |= GRN_CURSOR_GE;
			}
			else
			{
				*flags |= GRN_CURSOR_GT;
			}
			break;
		default:
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("unexpected strategy number for range search: %d",
							key->sk_strategy)));
			break;
		}
	}
}

static void
PGrnRangeSearch(IndexScanDesc scan, ScanDirection dir)
{
	PGrnScanOpaque so = (PGrnScanOpaque) scan->opaque;
	void *min = NULL;
	unsigned int minSize = 0;
	void *max = NULL;
	unsigned int maxSize = 0;
	int offset = 0;
	int limit = -1;
	int flags = 0;
	grn_id indexCursorMin = GRN_ID_NIL;
	grn_id indexCursorMax = GRN_ID_MAX;
	int indexCursorFlags = 0;
	grn_obj *indexColumn;
	grn_obj *lexicon;
	int i;
	unsigned int nthAttribute = 0;

	PGrnFillBorder(scan, &min, &minSize, &max, &maxSize, &flags);

	if (dir == BackwardScanDirection)
		flags |= GRN_CURSOR_DESCENDING;
	else
		flags |= GRN_CURSOR_ASCENDING;

	for (i = 0; i < scan->numberOfKeys; i++)
	{
		ScanKey key = &(scan->keyData[i]);
		nthAttribute = key->sk_attno - 1;
		break;
	}
	indexColumn = PGrnLookupIndexColumn(scan->indexRelation, nthAttribute,
										ERROR);
	lexicon = grn_column_table(ctx, indexColumn);

	so->tableCursor = grn_table_cursor_open(ctx, lexicon,
											min, minSize,
											max, maxSize,
											offset, limit, flags);
	so->indexCursor = grn_index_cursor_open(ctx,
											so->tableCursor, indexColumn,
											indexCursorMin,
											indexCursorMax,
											indexCursorFlags);
	so->keyAccessor = grn_obj_column(ctx, so->idsTable,
									 GRN_COLUMN_NAME_KEY,
									 GRN_COLUMN_NAME_KEY_LEN);
}

static bool
PGrnIsRangeSearchable(IndexScanDesc scan)
{
	int i;
	AttrNumber previousAttrNumber = InvalidAttrNumber;

	for (i = 0; i < scan->numberOfKeys; i++)
	{
		ScanKey key = &(scan->keyData[i]);

		if (previousAttrNumber == InvalidAttrNumber)
		{
			previousAttrNumber = key->sk_attno;
		}
		if (key->sk_attno != previousAttrNumber)
		{
			return false;
		}

		switch (key->sk_strategy)
		{
		case PGrnLessStrategyNumber:
		case PGrnLessEqualStrategyNumber:
		case PGrnGreaterStrategyNumber:
		case PGrnGreaterEqualStrategyNumber:
			break;
		default:
			return false;
			break;
		}
	}

	return true;
}

static void
PGrnEnsureCursorOpened(IndexScanDesc scan, ScanDirection dir)
{
	PGrnScanOpaque so = (PGrnScanOpaque) scan->opaque;

	if (so->indexCursor)
		return;
	if (so->tableCursor)
		return;

	if (PGrnIsRangeSearchable(scan))
	{
		PGrnRangeSearch(scan, dir);
	}
	else
	{
		PGrnSearch(scan);
		PGrnSort(scan);
		PGrnOpenTableCursor(scan, dir);
	}
}


/**
 * pgroonga.gettuple() -- amgettuple
 */
Datum
pgroonga_gettuple(PG_FUNCTION_ARGS)
{
	IndexScanDesc scan = (IndexScanDesc) PG_GETARG_POINTER(0);
	ScanDirection dir = (ScanDirection) PG_GETARG_INT32(1);
	PGrnScanOpaque so = (PGrnScanOpaque) scan->opaque;

	scan->xs_recheck = false;

	PGrnEnsureCursorOpened(scan, dir);

	if (scan->kill_prior_tuple && so->currentID != GRN_ID_NIL)
	{
		grn_obj key;
		GRN_UINT64_INIT(&key, 0);
		grn_obj_get_value(ctx, so->keyAccessor, so->currentID, &key);
		grn_table_delete(ctx, so->idsTable,
						 GRN_BULK_HEAD(&key), GRN_BULK_VSIZE(&key));
		GRN_OBJ_FIN(ctx, &key);
	}

	if (so->indexCursor)
	{
		grn_posting *posting;
		grn_id termID;
		grn_id id = GRN_ID_NIL;
		posting = grn_index_cursor_next(ctx, so->indexCursor, &termID);
		if (posting)
			id = posting->rid;
		so->currentID = id;
	}
	else
	{
		so->currentID = grn_table_cursor_next(ctx, so->tableCursor);
	}

	if (so->currentID == GRN_ID_NIL)
	{
		PG_RETURN_BOOL(false);
	}
	else
	{
		grn_obj key;

		GRN_UINT64_INIT(&key, 0);
		grn_obj_get_value(ctx, so->keyAccessor, so->currentID, &key);
		scan->xs_ctup.t_self = UInt64ToCtid(GRN_UINT64_VALUE(&key));
		GRN_OBJ_FIN(ctx, &key);

		PG_RETURN_BOOL(true);
	}

}

/**
 * pgroonga.getbitmap() -- amgetbitmap
 */
Datum
pgroonga_getbitmap(PG_FUNCTION_ARGS)
{
	IndexScanDesc scan = (IndexScanDesc) PG_GETARG_POINTER(0);
	TIDBitmap *tbm = (TIDBitmap *) PG_GETARG_POINTER(1);
	PGrnScanOpaque so = (PGrnScanOpaque) scan->opaque;
	int64 nRecords = 0;
	grn_obj key;

	PGrnEnsureCursorOpened(scan, ForwardScanDirection);

	GRN_UINT64_INIT(&key, 0);
	if (so->indexCursor)
	{
		grn_posting *posting;
		grn_id termID;
		while ((posting = grn_index_cursor_next(ctx, so->indexCursor, &termID)))
		{
			ItemPointerData ctid;
			GRN_BULK_REWIND(&key);
			grn_obj_get_value(ctx, so->keyAccessor, posting->rid, &key);
			ctid = UInt64ToCtid(GRN_UINT64_VALUE(&key));
			tbm_add_tuples(tbm, &ctid, 1, false);
			nRecords++;
		}
	}
	else
	{
		grn_id id;
		while ((id = grn_table_cursor_next(ctx, so->tableCursor)) != GRN_ID_NIL)
		{
			ItemPointerData ctid;
			GRN_BULK_REWIND(&key);
			grn_obj_get_value(ctx, so->keyAccessor, id, &key);
			ctid = UInt64ToCtid(GRN_UINT64_VALUE(&key));
			tbm_add_tuples(tbm, &ctid, 1, false);
			nRecords++;
		}
	}
	GRN_OBJ_FIN(ctx, &key);

	PG_RETURN_INT64(nRecords);
}

/**
 * pgroonga.rescan() -- amrescan
 */
Datum
pgroonga_rescan(PG_FUNCTION_ARGS)
{
	IndexScanDesc scan = (IndexScanDesc) PG_GETARG_POINTER(0);
	ScanKey	keys = (ScanKey) PG_GETARG_POINTER(1);
#ifdef NOT_USED
	int nkeys = PG_GETARG_INT32(2);
	ScanKey	orderbys = (ScanKey) PG_GETARG_POINTER(3);
	int norderbys = PG_GETARG_INT32(4);
#endif
	PGrnScanOpaque so = (PGrnScanOpaque) scan->opaque;

	PGrnScanOpaqueReinit(so);

	if (keys && scan->numberOfKeys > 0)
		memmove(scan->keyData, keys, scan->numberOfKeys * sizeof(ScanKeyData));

	PG_RETURN_VOID();
}

/**
 * pgroonga.endscan() -- amendscan
 */
Datum
pgroonga_endscan(PG_FUNCTION_ARGS)
{
	IndexScanDesc scan = (IndexScanDesc) PG_GETARG_POINTER(0);
	PGrnScanOpaque so = (PGrnScanOpaque) scan->opaque;

	PGrnScanOpaqueFin(so);
	pfree(so);

	PG_RETURN_VOID();
}

static void
PGrnBuildCallback(Relation index,
				  HeapTuple htup,
				  Datum *values,
				  bool *isnull,
				  bool tupleIsAlive,
				  void *state)
{
	PGrnBuildState bs = (PGrnBuildState) state;

	if (tupleIsAlive) {
		PGrnInsert(ctx, index, bs->idsTable, values, isnull, &(htup->t_self));
		bs->nIndexedTuples++;
	}
}

/**
 * pgroonga.build() -- ambuild
 */
Datum
pgroonga_build(PG_FUNCTION_ARGS)
{
	Relation heap = (Relation) PG_GETARG_POINTER(0);
	Relation index = (Relation) PG_GETARG_POINTER(1);
	IndexInfo *indexInfo = (IndexInfo *) PG_GETARG_POINTER(2);
	IndexBuildResult *result;
	double nHeapTuples = 0.0;
	PGrnBuildStateData bs;
	grn_obj lexicons;

	if (indexInfo->ii_Unique)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("pgroonga: unique index isn't supported")));

	bs.idsTable = NULL;
	bs.nIndexedTuples = 0.0;

	GRN_PTR_INIT(&lexicons, GRN_OBJ_VECTOR, GRN_ID_NIL);
	PG_TRY();
	{
		PGrnCreate(index, &(bs.idsTable), &lexicons);
		nHeapTuples = IndexBuildHeapScan(heap, index, indexInfo, true,
										 PGrnBuildCallback, &bs);
		PGrnSetSources(index, bs.idsTable);
	}
	PG_CATCH();
	{
		size_t i, nLexicons;

		nLexicons = GRN_BULK_VSIZE(&lexicons) / sizeof(grn_obj *);
		for (i = 0; i < nLexicons; i++)
		{
			grn_obj *lexicon = GRN_PTR_VALUE_AT(&lexicons, i);
			grn_obj_remove(ctx, lexicon);
		}
		GRN_OBJ_FIN(ctx, &lexicons);

		if (bs.idsTable)
			grn_obj_remove(ctx, bs.idsTable);

		PG_RE_THROW();
	}
	PG_END_TRY();
	GRN_OBJ_FIN(ctx, &lexicons);

	result = (IndexBuildResult *) palloc(sizeof(IndexBuildResult));
	result->heap_tuples = nHeapTuples;
	result->index_tuples = bs.nIndexedTuples;

	PG_RETURN_POINTER(result);
}

/**
 * pgroonga.buildempty() -- ambuildempty
 */
Datum
pgroonga_buildempty(PG_FUNCTION_ARGS)
{
	Relation index = (Relation) PG_GETARG_POINTER(0);
	grn_obj *idsTable = NULL;
	grn_obj lexicons;

	GRN_PTR_INIT(&lexicons, GRN_OBJ_VECTOR, GRN_ID_NIL);
	PG_TRY();
	{
		PGrnCreate(index, &idsTable, &lexicons);
		PGrnSetSources(index, idsTable);
	}
	PG_CATCH();
	{
		size_t i, nLexicons;

		nLexicons = GRN_BULK_VSIZE(&lexicons) / sizeof(grn_obj *);
		for (i = 0; i < nLexicons; i++)
		{
			grn_obj *lexicon = GRN_PTR_VALUE_AT(&lexicons, i);
			grn_obj_remove(ctx, lexicon);
		}
		GRN_OBJ_FIN(ctx, &lexicons);

		if (idsTable)
			grn_obj_remove(ctx, idsTable);

		PG_RE_THROW();
	}
	PG_END_TRY();
	GRN_OBJ_FIN(ctx, &lexicons);

	PG_RETURN_VOID();
}

static IndexBulkDeleteResult *
PGrnBulkDeleteResult(IndexVacuumInfo *info, grn_obj *idsTable)
{
	IndexBulkDeleteResult *stats;

	stats = (IndexBulkDeleteResult *) palloc0(sizeof(IndexBulkDeleteResult));
	stats->num_pages = (BlockNumber) 1;	/* TODO: sizeof index / BLCKSZ */

	/* table might be NULL if index is corrupted */
	if (idsTable)
		stats->num_index_tuples = grn_table_size(ctx, idsTable);
	else
		stats->num_index_tuples = 0;

	return stats;
}

/**
 * pgroonga.bulkdelete() -- ambulkdelete
 */
Datum
pgroonga_bulkdelete(PG_FUNCTION_ARGS)
{
	IndexVacuumInfo *info = (IndexVacuumInfo *) PG_GETARG_POINTER(0);
	IndexBulkDeleteResult *stats = (IndexBulkDeleteResult *) PG_GETARG_POINTER(1);
	IndexBulkDeleteCallback	callback = (IndexBulkDeleteCallback) PG_GETARG_POINTER(2);
	void *callback_state = (void *) PG_GETARG_POINTER(3);
	Relation index = info->index;
	grn_obj	*idsTable;
	grn_table_cursor *cursor;
	double nRemovedTuples;

	idsTable = PGrnLookupIDsTable(index, WARNING);

	if (!stats)
		stats = PGrnBulkDeleteResult(info, idsTable);

	if (!idsTable || !callback)
		PG_RETURN_POINTER(stats);

	nRemovedTuples = 0;

	cursor = grn_table_cursor_open(ctx, idsTable, NULL, 0, NULL, 0, 0, -1, 0);
	if (!cursor)
		ereport(ERROR,
				(errcode(ERRCODE_SYSTEM_ERROR),
				 errmsg("pgroonga: failed to open cursor: %s", ctx->errbuf)));

	PG_TRY();
	{
		while (grn_table_cursor_next(ctx, cursor) != GRN_ID_NIL)
		{
			ItemPointerData	ctid;
			uint64 *key;

			CHECK_FOR_INTERRUPTS();

			grn_table_cursor_get_key(ctx, cursor, (void **) &key);
			ctid = UInt64ToCtid(*key);
			if (callback(&ctid, callback_state))
			{
				grn_table_cursor_delete(ctx, cursor);

				nRemovedTuples += 1;
			}
		}
		grn_table_cursor_close(ctx, cursor);
	}
	PG_CATCH();
	{
		grn_table_cursor_close(ctx, cursor);
		PG_RE_THROW();
	}
	PG_END_TRY();

	stats->tuples_removed = nRemovedTuples;

	PG_RETURN_POINTER(stats);
}

static void
PGrnRemoveUnusedTables(void)
{
	grn_table_cursor *cursor;
	const char *min = PGrnIDsTableNamePrefix;

	cursor = grn_table_cursor_open(ctx, grn_ctx_db(ctx),
								   min, strlen(min),
								   NULL, 0,
								   0, -1, GRN_CURSOR_BY_KEY|GRN_CURSOR_PREFIX);
	while (grn_table_cursor_next(ctx, cursor) != GRN_ID_NIL) {
		char *name;
		char *nameEnd;
		int nameSize;
		Oid relationID;
		Relation relation;
		unsigned int i;

		nameSize = grn_table_cursor_get_key(ctx, cursor, (void **)&name);
		nameEnd = name + nameSize;
		relationID = strtol(name + strlen(min), &nameEnd, 10);
		relation = RelationIdGetRelation(relationID);
		if (relation)
		{
			RelationClose(relation);
			continue;
		}

		for (i = 0; true; i++)
		{
			char tableName[GRN_TABLE_MAX_KEY_SIZE];
			grn_obj *table;

			snprintf(tableName, sizeof(tableName),
					 PGrnLexiconNameFormat, i, relationID);
			table = grn_ctx_get(ctx, tableName, strlen(tableName));
			if (!table)
				break;
			grn_obj_remove(ctx, table);
		}

		{
			char tableName[GRN_TABLE_MAX_KEY_SIZE];
			grn_obj *table;

			snprintf(tableName, sizeof(tableName),
					 PGrnIDsTableNameFormat, relationID);
			table = grn_ctx_get(ctx, tableName, strlen(tableName));
			if (table)
			{
				grn_obj_remove(ctx, table);
			}
		}
	}
	grn_table_cursor_close(ctx, cursor);
}


/**
 * pgroonga.vacuumcleanup() -- amvacuumcleanup
 */
Datum
pgroonga_vacuumcleanup(PG_FUNCTION_ARGS)
{
	IndexVacuumInfo *info = (IndexVacuumInfo *) PG_GETARG_POINTER(0);
	IndexBulkDeleteResult *stats = (IndexBulkDeleteResult *) PG_GETARG_POINTER(1);

	if (!stats)
		stats = PGrnBulkDeleteResult(info,
									 PGrnLookupIDsTable(info->index, WARNING));

	PGrnRemoveUnusedTables();

	PG_RETURN_POINTER(stats);
}

/**
 * pgroonga.costestimate() -- amcostestimate
 */
Datum
pgroonga_costestimate(PG_FUNCTION_ARGS)
{
	/*
	 * We cannot use genericcostestimate because it is a static funciton.
	 * Use gistcostestimate instead, which just calls genericcostestimate.
	 */
	return gistcostestimate(fcinfo);
}

/**
 * pgroonga.options() -- amoptions
 */
Datum
pgroonga_options(PG_FUNCTION_ARGS)
{
	Datum reloptions = PG_GETARG_DATUM(0);
	bool validate = PG_GETARG_BOOL(1);
	relopt_value *options;
	PGrnOptions *grnOptions;
	int nOptions;
	const relopt_parse_elt optionsMap[] = {
		{"tokenizer", RELOPT_TYPE_STRING,
		 offsetof(PGrnOptions, tokenizerOffset)},
		{"normalizer", RELOPT_TYPE_STRING,
		 offsetof(PGrnOptions, normalizerOffset)}
	};

	options = parseRelOptions(reloptions, validate, PGrnReloptionKind,
							  &nOptions);
	grnOptions = allocateReloptStruct(sizeof(PGrnOptions), options, nOptions);
	fillRelOptions(grnOptions, sizeof(PGrnOptions), options, nOptions,
				   validate, optionsMap, lengthof(optionsMap));
	pfree(options);

	PG_RETURN_BYTEA_P(grnOptions);
}
