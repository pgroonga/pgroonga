#include "pgroonga.h"

#include "pgrn_compatible.h"

#include "pgrn_convert.h"
#include "pgrn_create.h"
#include "pgrn_global.h"
#include "pgrn_groonga.h"
#include "pgrn_highlight_html.h"
#include "pgrn_index_status.h"
#include "pgrn_jsonb.h"
#include "pgrn_keywords.h"
#include "pgrn_match_positions_byte.h"
#include "pgrn_match_positions_character.h"
#include "pgrn_options.h"
#include "pgrn_query_extract_keywords.h"
#include "pgrn_search.h"
#include "pgrn_value.h"
#include "pgrn_variables.h"
#include "pgrn_wal.h"

#ifdef PGRN_SUPPORT_CREATE_ACCESS_METHOD
#	include <access/amapi.h>
#endif
#ifdef PGRN_SUPPORT_OPTIONS
#	include <access/reloptions.h>
#endif
#include <access/relscan.h>
#include <catalog/catalog.h>
#include <catalog/index.h>
#include <catalog/pg_tablespace.h>
#include <catalog/pg_type.h>
#include <mb/pg_wchar.h>
#include <miscadmin.h>
#include <optimizer/clauses.h>
#include <optimizer/cost.h>
#include <postmaster/bgworker.h>
#include <storage/bufmgr.h>
#include <storage/ipc.h>
#include <storage/lmgr.h>
#include <utils/array.h>
#include <utils/builtins.h>
#include <utils/lsyscache.h>
#include <utils/selfuncs.h>
#include <utils/syscache.h>
#include <utils/timestamp.h>
#include <utils/tqual.h>
#include <utils/typcache.h>

#ifdef PGRN_SUPPORT_SCORE
#	include <lib/ilist.h>
#	include <utils/snapmgr.h>
#endif

#ifdef PGRN_SUPPORT_FILE_NODE_ID_TO_RELATION_ID
#	include <utils/relfilenodemap.h>
#endif

#include <groonga.h>

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifndef WIN32
#	include <unistd.h>
#endif

#ifdef WIN32
typedef struct _stat pgrn_stat_buffer;
#	define pgrn_stat(path, buffer) _stat(path, buffer)
#else
typedef struct stat pgrn_stat_buffer;
#	define pgrn_stat(path, buffer) stat(path, buffer)
#endif


PG_MODULE_MAGIC;

static bool PGrnInitialized = false;

typedef struct PGrnBuildStateData
{
	grn_obj	*sourcesTable;
	grn_obj	*sourcesCtidColumn;
	double nIndexedTuples;
	bool needMaxRecordSizeUpdate;
	uint32_t maxRecordSize;
} PGrnBuildStateData;

typedef PGrnBuildStateData *PGrnBuildState;

#ifdef PGRN_SUPPORT_SCORE
typedef struct PGrnPrimaryKeyColumn
{
	slist_node node;
	AttrNumber number;
	Oid type;
	grn_id domain;
	unsigned char flags;
	grn_obj *column;
} PGrnPrimaryKeyColumn;
#endif

typedef struct PGrnScanOpaqueData
{
	Relation index;
	Oid dataTableID;
	grn_obj *sourcesTable;
	grn_obj *sourcesCtidColumn;
	grn_obj minBorderValue;
	grn_obj maxBorderValue;
	grn_obj *searched;
	grn_obj *sorted;
	grn_obj *targetTable;
	grn_obj *indexCursor;
	grn_table_cursor *tableCursor;
	grn_obj *ctidAccessor;
	grn_obj *scoreAccessor;
	grn_id currentID;

#ifdef PGRN_SUPPORT_SCORE
	slist_node node;
	slist_head primaryKeyColumns;
#endif
} PGrnScanOpaqueData;

typedef PGrnScanOpaqueData *PGrnScanOpaque;

typedef struct PGrnMatchSequentialSearchData
{
	grn_obj *table;
	grn_obj *textColumn;
	grn_id recordID;
} PGrnMatchSequentialSearchData;

typedef struct PGrnPrefixRKSequentialSearchData
{
	grn_obj *table;
	grn_obj *key;
	grn_obj *resultTable;
} PGrnPrefixRKSequentialSearchData;

#ifdef PGRN_SUPPORT_SCORE
static slist_head PGrnScanOpaques = SLIST_STATIC_INIT(PGrnScanOpaques);
#endif

#ifdef PGRN_SUPPORT_SCORE
PG_FUNCTION_INFO_V1(pgroonga_score);
#endif
PG_FUNCTION_INFO_V1(pgroonga_table_name);
PG_FUNCTION_INFO_V1(pgroonga_command);

PG_FUNCTION_INFO_V1(pgroonga_match_term_text);
PG_FUNCTION_INFO_V1(pgroonga_match_term_text_array);
PG_FUNCTION_INFO_V1(pgroonga_match_term_varchar);
PG_FUNCTION_INFO_V1(pgroonga_match_term_varchar_array);
PG_FUNCTION_INFO_V1(pgroonga_match_query_text);
PG_FUNCTION_INFO_V1(pgroonga_match_query_text_array);
PG_FUNCTION_INFO_V1(pgroonga_match_query_varchar);
PG_FUNCTION_INFO_V1(pgroonga_match_regexp_text);
PG_FUNCTION_INFO_V1(pgroonga_match_regexp_varchar);

/* v2 */
PG_FUNCTION_INFO_V1(pgroonga_match_text);
PG_FUNCTION_INFO_V1(pgroonga_query_text);
PG_FUNCTION_INFO_V1(pgroonga_similar_text);
PG_FUNCTION_INFO_V1(pgroonga_script_text);
PG_FUNCTION_INFO_V1(pgroonga_prefix_text);
PG_FUNCTION_INFO_V1(pgroonga_prefix_rk_text);
PG_FUNCTION_INFO_V1(pgroonga_match_contain_text);
PG_FUNCTION_INFO_V1(pgroonga_query_contain_text);
PG_FUNCTION_INFO_V1(pgroonga_prefix_contain_text_array);
PG_FUNCTION_INFO_V1(pgroonga_prefix_rk_contain_text_array);

PG_FUNCTION_INFO_V1(pgroonga_insert);
PG_FUNCTION_INFO_V1(pgroonga_beginscan);
PG_FUNCTION_INFO_V1(pgroonga_gettuple);
#ifdef PGRN_SUPPORT_BITMAP_INDEX
PG_FUNCTION_INFO_V1(pgroonga_getbitmap);
#endif
PG_FUNCTION_INFO_V1(pgroonga_rescan);
PG_FUNCTION_INFO_V1(pgroonga_endscan);
PG_FUNCTION_INFO_V1(pgroonga_build);
PG_FUNCTION_INFO_V1(pgroonga_buildempty);
PG_FUNCTION_INFO_V1(pgroonga_bulkdelete);
PG_FUNCTION_INFO_V1(pgroonga_vacuumcleanup);
PG_FUNCTION_INFO_V1(pgroonga_canreturn);
PG_FUNCTION_INFO_V1(pgroonga_costestimate);
#ifdef PGRN_SUPPORT_CREATE_ACCESS_METHOD
PG_FUNCTION_INFO_V1(pgroonga_handler);
#endif

static grn_ctx *ctx = NULL;
static struct PGrnBuffers *buffers = &PGrnBuffers;
static PGrnMatchSequentialSearchData matchSequentialSearchData;
static PGrnPrefixRKSequentialSearchData prefixRKSequentialSearchData;

static uint32_t
PGrnGetThreadLimit(void *data)
{
	return 1;
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
	case PG_WIN1252:
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
	char *databasePath;
	char path[MAXPGPATH];
	grn_obj	*db;
	pgrn_stat_buffer file_status;

	GRN_CTX_SET_ENCODING(ctx, PGrnGetEncoding());
	databasePath = GetDatabasePath(MyDatabaseId, MyDatabaseTableSpace);
	join_path_components(path,
						 databasePath,
						 PGrnDatabaseBasename);
	pfree(databasePath);

	if (pgrn_stat(path, &file_status) == 0)
	{
		db = grn_db_open(ctx, path);
		if (!db)
			ereport(ERROR,
					(errcode(ERRCODE_IO_ERROR),
					 errmsg("pgroonga: failed to open database: <%s>: %s",
							path, ctx->errbuf)));
	}
	else
	{
		db = grn_db_create(ctx, path, NULL);
		if (!db)
			ereport(ERROR,
					(errcode(ERRCODE_IO_ERROR),
					 errmsg("pgroonga: failed to create database: <%s>: %s",
							path, ctx->errbuf)));
	}
}

static void
PGrnFinalizeMatchSequentialSearchData(void)
{
	grn_obj_close(ctx, matchSequentialSearchData.textColumn);
	grn_obj_close(ctx, matchSequentialSearchData.table);
}

static void
PGrnFinalizePrefixRKSequentialSearchData(void)
{
	grn_obj_close(ctx, prefixRKSequentialSearchData.resultTable);
	grn_obj_close(ctx, prefixRKSequentialSearchData.key);
	grn_obj_close(ctx, prefixRKSequentialSearchData.table);
}

static void
PGrnOnProcExit(int code, Datum arg)
{
	if (ctx)
	{
		grn_obj *db;

		PGrnFinalizeQueryExtractKeywords();

		PGrnFinalizeMatchPositionsByte();
		PGrnFinalizeMatchPositionsCharacter();

		PGrnFinalizeHighlightHTML();

		PGrnFinalizeKeywords();

		PGrnFinalizeJSONB();

		PGrnFinalizeMatchSequentialSearchData();
		PGrnFinalizePrefixRKSequentialSearchData();

		PGrnFinalizeBuffers();

		db = grn_ctx_db(ctx);
		if (db)
			grn_obj_close(ctx, db);

		grn_ctx_fin(ctx);
	}

	grn_fin();
}

static void
PGrnInitializeMatchSequentialSearchData(void)
{
	matchSequentialSearchData.table = grn_table_create(ctx,
													   NULL, 0,
													   NULL,
													   GRN_OBJ_TABLE_NO_KEY,
													   NULL, NULL);
	matchSequentialSearchData.textColumn =
		grn_column_create(ctx,
						  matchSequentialSearchData.table,
						  "text", strlen("text"),
						  NULL,
						  GRN_OBJ_COLUMN_SCALAR,
						  grn_ctx_at(ctx, GRN_DB_TEXT));
	matchSequentialSearchData.recordID =
		grn_table_add(ctx,
					  matchSequentialSearchData.table,
					  NULL, 0,
					  NULL);
}

static void
PGrnInitializePrefixRKSequentialSearchData(void)
{
	prefixRKSequentialSearchData.table =
		grn_table_create(ctx,
						 NULL, 0,
						 NULL,
						 GRN_OBJ_TABLE_PAT_KEY,
						 grn_ctx_at(ctx, GRN_DB_SHORT_TEXT),
						 NULL);

	prefixRKSequentialSearchData.key =
		grn_obj_column(ctx,
					   prefixRKSequentialSearchData.table,
					   GRN_COLUMN_NAME_KEY,
					   GRN_COLUMN_NAME_KEY_LEN);

	prefixRKSequentialSearchData.resultTable =
		grn_table_create(ctx,
						 NULL, 0,
						 NULL,
						 GRN_OBJ_TABLE_HASH_KEY | GRN_OBJ_WITH_SUBREC,
						 prefixRKSequentialSearchData.table,
						 NULL);
}

void
_PG_init(void)
{
	if (MyBgworkerEntry)
		return;

	if (PGrnInitialized)
		ereport(ERROR,
				(errcode(ERRCODE_SYSTEM_ERROR),
				 errmsg("pgroonga: already tried to initialize and failed")));

	PGrnInitialized = true;

	PGrnInitializeVariables();

	grn_thread_set_get_limit_func(PGrnGetThreadLimit, NULL);

	if (grn_init() != GRN_SUCCESS)
		ereport(ERROR,
				(errcode(ERRCODE_SYSTEM_ERROR),
				 errmsg("pgroonga: failed to initialize Groonga")));

	on_proc_exit(PGrnOnProcExit, 0);

	if (grn_ctx_init(&PGrnContext, 0))
		ereport(ERROR,
				(errcode(ERRCODE_SYSTEM_ERROR),
				 errmsg("pgroonga: failed to initialize Groonga context")));

	ctx = &PGrnContext;

	GRN_LOG(ctx, GRN_LOG_NOTICE, "pgroonga: initialize: <%s>", PGRN_VERSION);

	PGrnInitializeBuffers();

	PGrnEnsureDatabase();

	PGrnInitializeGroongaInformation();

	PGrnInitializeOptions();

	PGrnInitializeIndexStatus();

	PGrnInitializeMatchSequentialSearchData();
	PGrnInitializePrefixRKSequentialSearchData();

	PGrnInitializeJSONB();

	PGrnInitializeKeywords();

	PGrnInitializeHighlightHTML();

	PGrnInitializeMatchPositionsByte();
	PGrnInitializeMatchPositionsCharacter();

	PGrnInitializeQueryExtractKeywords();
}

static grn_id
PGrnPGTypeToGrnType(Oid pgTypeID, unsigned char *flags)
{
	grn_id typeID = GRN_ID_NIL;
	unsigned char typeFlags = 0;

	switch (pgTypeID)
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
		typeID = GRN_DB_TIME;
		break;
	case TEXTOID:
	case XMLOID:
		typeID = GRN_DB_LONG_TEXT;
		break;
	case VARCHAROID:
		typeID = GRN_DB_SHORT_TEXT;	/* 4KB */
		break;
#ifdef NOT_USED
	case POINTOID:
		typeID = GRN_DB_TOKYO_GEO_POINT or GRN_DB_WGS84_GEO_POINT;
		break;
#endif
	case VARCHARARRAYOID:
		typeID = GRN_DB_SHORT_TEXT;
		typeFlags |= GRN_OBJ_VECTOR;
		break;
	case TEXTARRAYOID:
		typeID = GRN_DB_LONG_TEXT;
		typeFlags |= GRN_OBJ_VECTOR;
		break;
	default:
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("pgroonga: unsupported type: %u", pgTypeID)));
		break;
	}

	if (flags)
	{
		*flags = typeFlags;
	}

	return typeID;
}

static grn_id
PGrnGetType(Relation index, AttrNumber n, unsigned char *flags)
{
	TupleDesc desc = RelationGetDescr(index);
	Form_pg_attribute attr;
	int32 maxLength;

	attr = desc->attrs[n];
	switch (attr->atttypid)
	{
	case VARCHAROID:
	case VARCHARARRAYOID:
		maxLength = type_maximum_size(VARCHAROID, attr->atttypmod);
		if (maxLength > 4096)
		{
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("pgroonga: "
							"4097bytes over size varchar isn't supported: %d",
							maxLength)));
		}
		break;
	default:
		break;
	}

	return PGrnPGTypeToGrnType(attr->atttypid, flags);
}

#ifdef PGRN_SUPPORT_INDEX_ONLY_SCAN
static Datum
PGrnConvertToDatumArrayType(grn_obj *vector, Oid typeID)
{
	Oid elementTypeID;
	int i, n;
	Datum *values;

	if (typeID == VARCHARARRAYOID)
		elementTypeID = VARCHAROID;
	else
		elementTypeID = TEXTOID;

	n = grn_vector_size(ctx, vector);
	if (n == 0)
		PG_RETURN_POINTER(construct_empty_array(elementTypeID));

	values = palloc(sizeof(Datum) * n);
	for (i = 0; i < n; i++)
	{
		const char *element;
		unsigned int elementSize;
		text *value;

		elementSize = grn_vector_get_element(ctx, vector, i,
											 &element,
											 NULL,
											 NULL);
		value = cstring_to_text_with_len(element, elementSize);
		values[i] = PointerGetDatum(value);
	}

	{
		int	dims[1];
		int	lbs[1];

		dims[0] = n;
		lbs[0] = 1;
		PG_RETURN_POINTER(construct_md_array(values, NULL,
											 1, dims, lbs,
											 elementTypeID,
											 -1, false, 'i'));
	}
}

static Datum
PGrnConvertToDatum(grn_obj *value, Oid typeID)
{
	switch (typeID)
	{
	case BOOLOID:
		PG_RETURN_BOOL(GRN_BOOL_VALUE(value));
		break;
	case INT2OID:
		PG_RETURN_INT16(GRN_INT16_VALUE(value));
		break;
	case INT4OID:
		PG_RETURN_INT32(GRN_INT32_VALUE(value));
		break;
	case INT8OID:
		PG_RETURN_INT64(GRN_INT64_VALUE(value));
		break;
	case FLOAT4OID:
		PG_RETURN_FLOAT4(GRN_FLOAT_VALUE(value));
		break;
	case FLOAT8OID:
		PG_RETURN_FLOAT8(GRN_FLOAT_VALUE(value));
		break;
	case TIMESTAMPOID:
	case TIMESTAMPTZOID:
	{
		int64 grnTime;
		int64 sec;
		int64 usec;
		pg_time_t unixTime;
		TimestampTz timestamp;

		grnTime = GRN_TIME_VALUE(value);
		GRN_TIME_UNPACK(grnTime, sec, usec);
		unixTime = sec;
		timestamp = time_t_to_timestamptz(unixTime);
#ifdef HAVE_INT64_TIMESTAMP
		timestamp += usec;
#else
		timestamp += ((double) used) / USECS_PER_SEC;
#endif
		if (typeID == TIMESTAMPOID)
			PG_RETURN_TIMESTAMP(timestamp);
		else
			PG_RETURN_TIMESTAMPTZ(timestamp);
		break;
	}
	case TEXTOID:
	case XMLOID:
	{
		text *text = cstring_to_text_with_len(GRN_TEXT_VALUE(value),
											  GRN_TEXT_LEN(value));
		PG_RETURN_TEXT_P(text);
		break;
	}
	case VARCHAROID:
	{
		text *text = cstring_to_text_with_len(GRN_TEXT_VALUE(value),
											  GRN_TEXT_LEN(value));
		PG_RETURN_VARCHAR_P((VarChar *) text);
		break;
	}
#ifdef NOT_USED
	case POINTOID:
		/* GRN_DB_TOKYO_GEO_POINT or GRN_DB_WGS84_GEO_POINT; */
		break;
#endif
	case VARCHARARRAYOID:
	case TEXTARRAYOID:
		return PGrnConvertToDatumArrayType(value, typeID);
		break;
	default:
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("pgroonga: unsupported datum type: %u",
						typeID)));
		break;
	}
}
#endif

static bool
PGrnIsQueryStrategyIndex(Relation index, int nthAttribute)
{
	Oid strategyOID;
	Oid leftType;
	Oid rightType;

	leftType = index->rd_opcintype[nthAttribute];

	switch (leftType)
	{
	case VARCHARARRAYOID:
		rightType = VARCHAROID;
		break;
	case TEXTARRAYOID:
		rightType = TEXTOID;
		break;
	default:
		rightType = leftType;
		break;
	}

	strategyOID = get_opfamily_member(index->rd_opfamily[nthAttribute],
									  leftType,
									  rightType,
									  PGrnQueryStrategyNumber);
	if (OidIsValid(strategyOID))
		return true;

	strategyOID = get_opfamily_member(index->rd_opfamily[nthAttribute],
									  leftType,
									  rightType,
									  PGrnQueryStrategyV2Number);
	if (OidIsValid(strategyOID))
		return true;

	return false;
}

static bool
PGrnIsQueryContainStrategyIndex(Relation index, int nthAttribute)
{
	Oid strategyOID;
	Oid leftType;
	Oid rightType;

	leftType = index->rd_opcintype[nthAttribute];

	switch (leftType)
	{
	case VARCHAROID:
		rightType = VARCHARARRAYOID;
		break;
	case TEXTOID:
		rightType = TEXTARRAYOID;
		break;
	default:
		rightType = leftType;
		break;
	}

	strategyOID = get_opfamily_member(index->rd_opfamily[nthAttribute],
									  leftType,
									  rightType,
									  PGrnQueryContainStrategyV2Number);
	return OidIsValid(strategyOID);
}

static bool
PGrnIsScriptStrategyIndex(Relation index, int nthAttribute)
{
	Oid strategyOID;
	Oid leftType;
	Oid rightType;

	leftType = index->rd_opcintype[nthAttribute];

	switch (leftType)
	{
	case VARCHARARRAYOID:
		rightType = VARCHAROID;
		break;
	case TEXTARRAYOID:
		rightType = TEXTOID;
		break;
	default:
		rightType = leftType;
		break;
	}

	strategyOID = get_opfamily_member(index->rd_opfamily[nthAttribute],
									  leftType,
									  rightType,
									  PGrnScriptStrategyV2Number);
	if (OidIsValid(strategyOID))
		return true;

	return false;
}

static bool
PGrnIsForFullTextSearchIndex(Relation index, int nthAttribute)
{
	if (PGrnIsQueryStrategyIndex(index, nthAttribute))
		return true;

	if (PGrnIsQueryContainStrategyIndex(index, nthAttribute))
		return true;

	if (PGrnIsScriptStrategyIndex(index, nthAttribute))
		return true;

	return false;
}

static bool
PGrnIsForRegexpSearchIndex(Relation index, int nthAttribute)
{
	Oid regexpStrategyOID;
	Oid leftType;
	Oid rightType;

	leftType = index->rd_opcintype[nthAttribute];
	rightType = leftType;
	regexpStrategyOID = get_opfamily_member(index->rd_opfamily[nthAttribute],
											leftType,
											rightType,
											PGrnRegexpStrategyNumber);
	return OidIsValid(regexpStrategyOID);
}

static bool
PGrnIsForPrefixSearchIndex(Relation index, int nthAttribute)
{
	Oid prefixStrategyOID;
	Oid prefixContainStrategyOID;
	Oid leftType;
	Oid rightType;

	leftType = index->rd_opcintype[nthAttribute];

	switch (leftType)
	{
	case VARCHARARRAYOID:
		rightType = VARCHAROID;
		break;
	case TEXTARRAYOID:
		rightType = TEXTOID;
		break;
	default:
		rightType = leftType;
		break;
	}

	prefixStrategyOID = get_opfamily_member(index->rd_opfamily[nthAttribute],
											leftType,
											rightType,
											PGrnPrefixStrategyV2Number);
	if (OidIsValid(prefixStrategyOID))
		return true;

	prefixContainStrategyOID =
		get_opfamily_member(index->rd_opfamily[nthAttribute],
							leftType,
							rightType,
							PGrnPrefixContainStrategyV2Number);
	if (OidIsValid(prefixContainStrategyOID))
		return true;

	return false;
}

/**
 * PGrnCreate
 */
static void
PGrnCreate(Relation index,
		   grn_obj **sourcesTable,
		   grn_obj **sourcesCtidColumn,
		   grn_obj *supplementaryTables,
		   grn_obj *lexicons)
{
	PGrnCreateData data;

	data.index = index;
	data.desc = RelationGetDescr(index);
	data.relNode = index->rd_node.relNode;
	data.supplementaryTables = supplementaryTables;
	data.lexicons = lexicons;

	PGrnCreateSourcesTable(&data);
	*sourcesTable = data.sourcesTable;
	*sourcesCtidColumn = data.sourcesCtidColumn;

	for (data.i = 0; data.i < data.desc->natts; data.i++)
	{
		Form_pg_attribute attribute;

		attribute = data.desc->attrs[data.i];
		if (PGrnAttributeIsJSONB(attribute->atttypid))
		{
			data.forFullTextSearch = false;
			data.forRegexpSearch = false;
			data.forPrefixSearch = false;
			PGrnJSONBCreate(&data);
		}
		else
		{
			data.forFullTextSearch = PGrnIsForFullTextSearchIndex(index, data.i);
			data.forRegexpSearch = PGrnIsForRegexpSearchIndex(index, data.i);
			data.forPrefixSearch = PGrnIsForPrefixSearchIndex(index, data.i);
			data.attributeTypeID = PGrnGetType(index, data.i,
											   &(data.attributeFlags));
			PGrnCreateLexicon(&data);
			PGrnCreateDataColumn(&data);
			PGrnCreateIndexColumn(&data);
		}
	}
}

static void
PGrnSetSources(Relation index, grn_obj *sourcesTable)
{
	TupleDesc desc;
	unsigned int i;

	desc = RelationGetDescr(index);
	for (i = 0; i < desc->natts; i++)
	{
		Form_pg_attribute attribute = desc->attrs[i];
		NameData *name = &(attribute->attname);
		grn_obj *source;
		grn_obj *indexColumn;

		if (PGrnAttributeIsJSONB(attribute->atttypid))
		{
			indexColumn = PGrnJSONBSetSource(index, i);
		}
		else
		{
			indexColumn = PGrnLookupIndexColumn(index, i, ERROR);
		}

		source = PGrnLookupColumn(sourcesTable, name->data, ERROR);
		PGrnIndexColumnSetSource(index, indexColumn, source);
		grn_obj_unlink(ctx, source);
		grn_obj_unlink(ctx, indexColumn);
	}
}

static uint64
CtidToUInt64(ItemPointer ctid)
{
	BlockNumber blockNumber;
	OffsetNumber offsetNumber;

	blockNumber = ItemPointerGetBlockNumber(ctid);
	offsetNumber = ItemPointerGetOffsetNumber(ctid);
	return (((uint64)blockNumber << 16) | ((uint64)offsetNumber));
}

static ItemPointerData
UInt64ToCtid(uint64 key)
{
	ItemPointerData	ctid;
	ItemPointerSet(&ctid, (key >> 16) & 0xFFFFFFFF, key & 0xFFFF);
	return ctid;
}

#ifdef PGRN_SUPPORT_SCORE
static bool
PGrnIsAliveCtid(Relation table, ItemPointer ctid)
{
	Buffer buffer;
	HeapTupleData tuple;
	Snapshot snapshot;
	ItemPointerData realCtid;
	bool allDead;
	bool found;
	bool isAlive = false;

	buffer = ReadBuffer(table, ItemPointerGetBlockNumber(ctid));
	snapshot = RegisterSnapshot(GetLatestSnapshot());
	realCtid = *ctid;
	found = heap_hot_search_buffer(&realCtid, table, buffer, snapshot, &tuple,
								   &allDead, true);
	if (found) {
		uint64 tupleID;

		tupleID = CtidToUInt64(&(tuple.t_self));
		isAlive = (tupleID == CtidToUInt64(ctid) ||
				   tupleID == CtidToUInt64(&realCtid));
	}
	UnregisterSnapshot(snapshot);
	ReleaseBuffer(buffer);

	return isAlive;
}

static double
PGrnCollectScoreScanOpaque(Relation table, HeapTuple tuple, PGrnScanOpaque so)
{
	double score = 0.0;
	TupleDesc desc;
	grn_obj *records;
	grn_obj *expression;
	grn_obj *variable;
	slist_iter iter;
	unsigned int nPrimaryKeyColumns = 0;

	if (so->dataTableID != tuple->t_tableOid)
		return 0.0;

	if (!so->scoreAccessor)
		return 0.0;

	if (slist_is_empty(&(so->primaryKeyColumns)))
		return 0.0;

	desc = RelationGetDescr(table);

	records = grn_table_create(ctx, NULL, 0, NULL,
							   GRN_OBJ_TABLE_HASH_KEY | GRN_OBJ_WITH_SUBREC,
							   so->sourcesTable, NULL);
	GRN_EXPR_CREATE_FOR_QUERY(ctx, so->sourcesTable, expression, variable);

	slist_foreach(iter, &(so->primaryKeyColumns))
	{
		PGrnPrimaryKeyColumn *primaryKeyColumn;
		bool isNULL;
		Datum primaryKeyValue;

		primaryKeyColumn = slist_container(PGrnPrimaryKeyColumn, node, iter.cur);

		grn_obj_reinit(ctx,
					   &(buffers->general),
					   primaryKeyColumn->domain,
					   primaryKeyColumn->flags);

		primaryKeyValue = heap_getattr(tuple,
									   primaryKeyColumn->number,
									   desc,
									   &isNULL);
		PGrnConvertFromData(primaryKeyValue,
							primaryKeyColumn->type,
							&(buffers->general));

		grn_expr_append_obj(ctx, expression,
							primaryKeyColumn->column, GRN_OP_PUSH, 1);
		grn_expr_append_op(ctx, expression, GRN_OP_GET_VALUE, 1);
		grn_expr_append_const(ctx, expression,
							  &(buffers->general), GRN_OP_PUSH, 1);
		grn_expr_append_op(ctx, expression, GRN_OP_EQUAL, 2);

		if (nPrimaryKeyColumns > 0)
			grn_expr_append_op(ctx, expression, GRN_OP_AND, 2);
		nPrimaryKeyColumns++;
	}
	grn_table_select(ctx,
					 so->sourcesTable,
					 expression,
					 records,
					 GRN_OP_OR);
	grn_obj_close(ctx, expression);

	{
		grn_table_cursor *tableCursor;

		tableCursor = grn_table_cursor_open(ctx, records,
											NULL, 0, NULL, 0,
											0, -1, GRN_CURSOR_ASCENDING);
		while (grn_table_cursor_next(ctx, tableCursor) != GRN_ID_NIL)
		{
			grn_id recordID;
			grn_id id;
			ItemPointerData ctid;

			{
				void *key;
				grn_table_cursor_get_key(ctx, tableCursor, &key);
				recordID = *((grn_id *) key);
			}
			id = grn_table_get(ctx, so->searched, &recordID, sizeof(grn_id));
			if (id == GRN_ID_NIL)
				continue;

			GRN_BULK_REWIND(&(buffers->ctid));
			grn_obj_get_value(ctx, so->ctidAccessor, id, &(buffers->ctid));
			ctid = UInt64ToCtid(GRN_UINT64_VALUE(&(buffers->ctid)));

			if (!PGrnIsAliveCtid(table, &ctid))
				continue;

			GRN_BULK_REWIND(&(buffers->score));
			grn_obj_get_value(ctx, so->scoreAccessor, id, &(buffers->score));
			if (buffers->score.header.domain == GRN_DB_FLOAT)
			{
				score += GRN_FLOAT_VALUE(&(buffers->score));
			}
			else
			{
				score += GRN_INT32_VALUE(&(buffers->score));
			}
		}
		grn_obj_unlink(ctx, tableCursor);
	}

	grn_obj_close(ctx, records);

	return score;
}

static double
PGrnCollectScore(Relation table, HeapTuple tuple)
{
	double score = 0.0;
	slist_iter iter;

	slist_foreach(iter, &PGrnScanOpaques)
	{
		PGrnScanOpaque so;

		so = slist_container(PGrnScanOpaqueData, node, iter.cur);
		score += PGrnCollectScoreScanOpaque(table, tuple, so);
	}

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

	if (desc->natts > 0 && !slist_is_empty(&PGrnScanOpaques))
	{
		HeapTupleData tupleData;
		HeapTuple tuple;
		Relation table;

		tupleData.t_len = HeapTupleHeaderGetDatumLength(header);
		tupleData.t_tableOid = desc->attrs[0]->attrelid;
		tupleData.t_data = header;
		tuple = &tupleData;

		table = RelationIdGetRelation(tuple->t_tableOid);

		score = PGrnCollectScore(table, tuple);

		RelationClose(table);
	}

	ReleaseTupleDesc(desc);

	PG_RETURN_FLOAT8(score);
}
#endif

/**
 * pgroonga.table_name(indexName cstring) : cstring
 */
Datum
pgroonga_table_name(PG_FUNCTION_ARGS)
{
	Datum indexNameDatum = PG_GETARG_DATUM(0);
	Datum indexOidDatum;
	Oid indexOid;
	Oid fileNodeOid;
	char tableName[GRN_TABLE_MAX_KEY_SIZE];
	char *copiedTableName;

	indexOidDatum = DirectFunctionCall1(regclassin, indexNameDatum);
	if (!OidIsValid(indexOidDatum))
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_NAME),
				 errmsg("pgroonga: unknown index name: <%s>",
						DatumGetCString(indexNameDatum))));
	}
	indexOid = DatumGetObjectId(indexOidDatum);

	{
		HeapTuple tuple;
		tuple = SearchSysCache1(RELOID, indexOid);
		if (!HeapTupleIsValid(tuple)) {
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_NAME),
					 errmsg("pgroonga: "
							"failed to find file node ID from index name: <%s>",
							DatumGetCString(indexNameDatum))));
		}
		{
			Form_pg_class indexClass = (Form_pg_class) GETSTRUCT(tuple);
			fileNodeOid = indexClass->relfilenode;
			ReleaseSysCache(tuple);
		}
	}

	snprintf(tableName, sizeof(tableName),
			 PGrnSourcesTableNameFormat,
			 fileNodeOid);
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

	GRN_BULK_REWIND(&(buffers->body));
	do {
		char *chunk;
		unsigned int chunkSize;
		grn_ctx_recv(ctx, &chunk, &chunkSize, &flags);
		GRN_TEXT_PUT(ctx, &(buffers->body), chunk, chunkSize);
	} while ((flags & GRN_CTX_MORE));

	GRN_BULK_REWIND(&(buffers->head));
	GRN_BULK_REWIND(&(buffers->foot));
	grn_output_envelope(ctx,
						rc,
						&(buffers->head),
						&(buffers->body),
						&(buffers->foot),
						NULL,
						0);

	grn_obj_reinit(ctx, &(buffers->general), GRN_DB_TEXT, 0);
	GRN_TEXT_PUT(ctx, &(buffers->general),
				 GRN_TEXT_VALUE(&(buffers->head)),
				 GRN_TEXT_LEN(&(buffers->head)));
	GRN_TEXT_PUT(ctx, &(buffers->general),
				 GRN_TEXT_VALUE(&(buffers->body)),
				 GRN_TEXT_LEN(&(buffers->body)));
	GRN_TEXT_PUT(ctx, &(buffers->general),
				 GRN_TEXT_VALUE(&(buffers->foot)),
				 GRN_TEXT_LEN(&(buffers->foot)));
	result = cstring_to_text_with_len(GRN_TEXT_VALUE(&(buffers->general)),
									  GRN_TEXT_LEN(&(buffers->general)));
	PG_RETURN_TEXT_P(result);
}

static grn_bool
pgroonga_match_term_raw(const char *text, unsigned int textSize,
						const char *term, unsigned int termSize)
{
	grn_bool matched;
	grn_obj targetBuffer;
	grn_obj termBuffer;

	GRN_TEXT_INIT(&targetBuffer, GRN_OBJ_DO_SHALLOW_COPY);
	GRN_TEXT_SET(ctx, &targetBuffer, text, textSize);

	GRN_TEXT_INIT(&termBuffer, GRN_OBJ_DO_SHALLOW_COPY);
	GRN_TEXT_SET(ctx, &termBuffer, term, termSize);

	matched = grn_operator_exec_match(ctx, &targetBuffer, &termBuffer);

	GRN_OBJ_FIN(ctx, &targetBuffer);
	GRN_OBJ_FIN(ctx, &termBuffer);

	return matched;
}

/**
 * pgroonga.match_term(target text, term text) : bool
 */
Datum
pgroonga_match_term_text(PG_FUNCTION_ARGS)
{
	text *target = PG_GETARG_TEXT_PP(0);
	text *term = PG_GETARG_TEXT_PP(1);
	grn_bool matched;

	matched = pgroonga_match_term_raw(VARDATA_ANY(target),
									  VARSIZE_ANY_EXHDR(target),
									  VARDATA_ANY(term),
									  VARSIZE_ANY_EXHDR(term));
	PG_RETURN_BOOL(matched);
}

/**
 * pgroonga.match_term(target text[], term text) : bool
 */
Datum
pgroonga_match_term_text_array(PG_FUNCTION_ARGS)
{
	ArrayType *target = PG_GETARG_ARRAYTYPE_P(0);
	text *term = PG_GETARG_TEXT_PP(1);
	bool matched = false;
	grn_obj elementBuffer;
	int i, n;

	grn_obj_reinit(ctx, &(buffers->general), GRN_DB_TEXT, 0);
	GRN_TEXT_SET(ctx, &(buffers->general), VARDATA_ANY(term), VARSIZE_ANY_EXHDR(term));

	GRN_TEXT_INIT(&elementBuffer, GRN_OBJ_DO_SHALLOW_COPY);

	n = ARR_DIMS(target)[0];
	for (i = 1; i <= n; i++)
	{
		Datum elementDatum;
		text *element;
		bool isNULL;

		elementDatum = array_ref(target, 1, &i, -1, -1, false, 'i', &isNULL);
		if (isNULL)
			continue;

		element = DatumGetTextPP(elementDatum);
		GRN_TEXT_SET(ctx, &elementBuffer,
					 VARDATA_ANY(element), VARSIZE_ANY_EXHDR(element));
		if (pgroonga_match_term_raw(GRN_TEXT_VALUE(&elementBuffer),
									GRN_TEXT_LEN(&elementBuffer),
									GRN_TEXT_VALUE(&(buffers->general)),
									GRN_TEXT_LEN(&(buffers->general))))
		{
			matched = true;
			break;
		}
	}

	GRN_OBJ_FIN(ctx, &elementBuffer);

	PG_RETURN_BOOL(matched);
}

/**
 * pgroonga.match_term(target varchar, term varchar) : bool
 */
Datum
pgroonga_match_term_varchar(PG_FUNCTION_ARGS)
{
	VarChar *target = PG_GETARG_VARCHAR_PP(0);
	VarChar *term = PG_GETARG_VARCHAR_PP(1);
	grn_bool matched;

	matched =
		pgroonga_match_term_raw(VARDATA_ANY(target), VARSIZE_ANY_EXHDR(target),
								VARDATA_ANY(term), VARSIZE_ANY_EXHDR(term));
	PG_RETURN_BOOL(matched);
}

/**
 * pgroonga.match_term(target varchar[], term varchar) : bool
 */
Datum
pgroonga_match_term_varchar_array(PG_FUNCTION_ARGS)
{
	ArrayType *target = PG_GETARG_ARRAYTYPE_P(0);
	VarChar *term = PG_GETARG_VARCHAR_PP(1);
	bool matched = false;
	grn_obj elementBuffer;
	int i, n;

	grn_obj_reinit(ctx, &(buffers->general), GRN_DB_TEXT, 0);
	GRN_TEXT_SET(ctx, &(buffers->general), VARDATA_ANY(term), VARSIZE_ANY_EXHDR(term));

	GRN_TEXT_INIT(&elementBuffer, GRN_OBJ_DO_SHALLOW_COPY);

	n = ARR_DIMS(target)[0];
	for (i = 1; i <= n; i++)
	{
		Datum elementDatum;
		VarChar *element;
		bool isNULL;

		elementDatum = array_ref(target, 1, &i, -1, -1, false, 'i', &isNULL);
		if (isNULL)
			continue;

		element = DatumGetVarCharPP(elementDatum);
		GRN_TEXT_SET(ctx, &elementBuffer,
					 VARDATA_ANY(element), VARSIZE_ANY_EXHDR(element));
		if (grn_operator_exec_equal(ctx, &(buffers->general), &elementBuffer))
		{
			matched = true;
			break;
		}
	}

	GRN_OBJ_FIN(ctx, &elementBuffer);

	PG_RETURN_BOOL(matched);
}

static grn_bool
pgroonga_match_query_raw(const char *target, unsigned int targetSize,
						 const char *query, unsigned int querySize)
{
	grn_obj *expression;
	grn_obj *variable;
	grn_expr_flags flags =
		GRN_EXPR_SYNTAX_QUERY | GRN_EXPR_ALLOW_LEADING_NOT;
	grn_rc rc;
	grn_obj *result;
	bool matched = false;

	GRN_EXPR_CREATE_FOR_QUERY(ctx,
							  matchSequentialSearchData.table,
							  expression,
							  variable);
	if (!expression)
	{
		ereport(ERROR,
				(errcode(ERRCODE_OUT_OF_MEMORY),
				 errmsg("pgroonga: failed to create expression: %s",
						ctx->errbuf)));
	}

	rc = grn_expr_parse(ctx,
						expression,
						query, querySize,
						matchSequentialSearchData.textColumn,
						GRN_OP_MATCH, GRN_OP_AND,
						flags);
	if (rc != GRN_SUCCESS)
	{
		char message[GRN_CTX_MSGSIZE];
		grn_strncpy(message, GRN_CTX_MSGSIZE,
					ctx->errbuf, GRN_CTX_MSGSIZE);

		grn_obj_close(ctx, expression);
		ereport(ERROR,
				(errcode(PGrnRCToPgErrorCode(rc)),
				 errmsg("pgroonga: failed to parse expression: %s",
						message)));
	}

	grn_obj_reinit(ctx, &(buffers->general), GRN_DB_TEXT, 0);
	GRN_TEXT_SET(ctx, &(buffers->general), target, targetSize);
	grn_obj_set_value(ctx,
					  matchSequentialSearchData.textColumn,
					  matchSequentialSearchData.recordID,
					  &(buffers->general),
					  GRN_OBJ_SET);
	GRN_RECORD_SET(ctx, variable, matchSequentialSearchData.recordID);

	result = grn_expr_exec(ctx, expression, 0);
	GRN_OBJ_IS_TRUE(ctx, result, matched);

	grn_obj_close(ctx, expression);

	return matched;
}

/**
 * pgroonga.match_query(target text, query text) : bool
 */
Datum
pgroonga_match_query_text(PG_FUNCTION_ARGS)
{
	text *target = PG_GETARG_TEXT_PP(0);
	text *query = PG_GETARG_TEXT_PP(1);
	bool matched = false;

	matched = pgroonga_match_query_raw(VARDATA_ANY(target),
									   VARSIZE_ANY_EXHDR(target),
									   VARDATA_ANY(query),
									   VARSIZE_ANY_EXHDR(query));

	PG_RETURN_BOOL(matched);
}

/**
 * pgroonga.match_query(targets text[], query text) : bool
 */
Datum
pgroonga_match_query_text_array(PG_FUNCTION_ARGS)
{
	ArrayType *targets = PG_GETARG_ARRAYTYPE_P(0);
	text *query = PG_GETARG_TEXT_PP(1);
	bool matched = false;
	int i, n;

	n = ARR_DIMS(targets)[0];
	for (i = 1; i <= n; i++)
	{
		Datum targetDatum;
		text *target;
		bool isNULL;

		targetDatum = array_ref(targets, 1, &i, -1, -1, false, 'i', &isNULL);
		if (isNULL)
			continue;

		target = DatumGetTextPP(targetDatum);
		matched = pgroonga_match_query_raw(VARDATA_ANY(target),
										   VARSIZE_ANY_EXHDR(target),
										   VARDATA_ANY(query),
										   VARSIZE_ANY_EXHDR(query));
		if (matched)
		{
			break;
		}
	}

	PG_RETURN_BOOL(matched);
}

/**
 * pgroonga.match_query(target varchar, query varchar) : bool
 */
Datum
pgroonga_match_query_varchar(PG_FUNCTION_ARGS)
{
	VarChar *target = PG_GETARG_VARCHAR_PP(0);
	VarChar *query = PG_GETARG_VARCHAR_PP(1);
	bool matched = false;

	matched = pgroonga_match_query_raw(VARDATA_ANY(target),
									   VARSIZE_ANY_EXHDR(target),
									   VARDATA_ANY(query),
									   VARSIZE_ANY_EXHDR(query));

	PG_RETURN_BOOL(matched);
}

static grn_bool
pgroonga_match_regexp_raw(const char *text, unsigned int textSize,
						  const char *pattern, unsigned int patternSize)
{
	grn_bool matched;
	grn_obj targetBuffer;
	grn_obj patternBuffer;

	GRN_TEXT_INIT(&targetBuffer, GRN_OBJ_DO_SHALLOW_COPY);
	GRN_TEXT_SET(ctx, &targetBuffer, text, textSize);

	GRN_TEXT_INIT(&patternBuffer, GRN_OBJ_DO_SHALLOW_COPY);
	GRN_TEXT_SET(ctx, &patternBuffer, pattern, patternSize);

	matched = grn_operator_exec_regexp(ctx, &targetBuffer, &patternBuffer);

	GRN_OBJ_FIN(ctx, &targetBuffer);
	GRN_OBJ_FIN(ctx, &patternBuffer);

	return matched;
}

/**
 * pgroonga.match_regexp(target text, pattern text) : bool
 */
Datum
pgroonga_match_regexp_text(PG_FUNCTION_ARGS)
{
	text *target = PG_GETARG_TEXT_PP(0);
	text *pattern = PG_GETARG_TEXT_PP(1);
	grn_bool matched;

	matched = pgroonga_match_regexp_raw(VARDATA_ANY(target),
										VARSIZE_ANY_EXHDR(target),
										VARDATA_ANY(pattern),
										VARSIZE_ANY_EXHDR(pattern));
	PG_RETURN_BOOL(matched);
}

/**
 * pgroonga.match_regexp_varchar(target varchar, pattern varchar) : bool
 */
Datum
pgroonga_match_regexp_varchar(PG_FUNCTION_ARGS)
{
	VarChar *target = PG_GETARG_VARCHAR_PP(0);
	VarChar *pattern = PG_GETARG_VARCHAR_PP(1);
	grn_bool matched;

	matched = pgroonga_match_regexp_raw(VARDATA_ANY(target),
										VARSIZE_ANY_EXHDR(target),
										VARDATA_ANY(pattern),
										VARSIZE_ANY_EXHDR(pattern));
	PG_RETURN_BOOL(matched);
}

/* v2 */
/**
 * pgroonga.match_text(target text, term text) : bool
 */
Datum
pgroonga_match_text(PG_FUNCTION_ARGS)
{
	text *target = PG_GETARG_TEXT_PP(0);
	text *term = PG_GETARG_TEXT_PP(1);
	grn_bool matched;

	matched = pgroonga_match_term_raw(VARDATA_ANY(target),
									  VARSIZE_ANY_EXHDR(target),
									  VARDATA_ANY(term),
									  VARSIZE_ANY_EXHDR(term));
	PG_RETURN_BOOL(matched);
}

/**
 * pgroonga.query_text(target text, query text) : bool
 */
Datum
pgroonga_query_text(PG_FUNCTION_ARGS)
{
	text *target = PG_GETARG_TEXT_PP(0);
	text *query = PG_GETARG_TEXT_PP(1);
	bool matched = false;

	matched = pgroonga_match_query_raw(VARDATA_ANY(target),
									   VARSIZE_ANY_EXHDR(target),
									   VARDATA_ANY(query),
									   VARSIZE_ANY_EXHDR(query));

	PG_RETURN_BOOL(matched);
}

/**
 * pgroonga.similar_text(target text, document text) : bool
 */
Datum
pgroonga_similar_text(PG_FUNCTION_ARGS)
{
#ifdef NOT_USED
	text *target = PG_GETARG_TEXT_PP(0);
	text *document = PG_GETARG_TEXT_PP(1);
	bool matched = false;

	matched = pgroonga_similar_raw(VARDATA_ANY(target),
								   VARSIZE_ANY_EXHDR(target),
								   VARDATA_ANY(document),
								   VARSIZE_ANY_EXHDR(document));

	PG_RETURN_BOOL(matched);
#endif

	ereport(ERROR,
			(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
			 errmsg("pgroonga: operator &~? is available only in index scan")));

	PG_RETURN_BOOL(false);
}

static grn_bool
pgroonga_script_raw(const char *target, unsigned int targetSize,
					const char *script, unsigned int scriptSize)
{
	grn_obj *expression;
	grn_obj *variable;
	grn_expr_flags flags = GRN_EXPR_SYNTAX_SCRIPT;
	grn_rc rc;
	grn_obj *result;
	bool matched = false;

	GRN_EXPR_CREATE_FOR_QUERY(ctx,
							  matchSequentialSearchData.table,
							  expression,
							  variable);
	if (!expression)
	{
		ereport(ERROR,
				(errcode(ERRCODE_OUT_OF_MEMORY),
				 errmsg("pgroonga: failed to create expression: %s",
						ctx->errbuf)));
	}

	rc = grn_expr_parse(ctx,
						expression,
						script, scriptSize,
						matchSequentialSearchData.textColumn,
						GRN_OP_MATCH, GRN_OP_AND,
						flags);
	if (rc != GRN_SUCCESS)
	{
		char message[GRN_CTX_MSGSIZE];
		grn_strncpy(message, GRN_CTX_MSGSIZE,
					ctx->errbuf, GRN_CTX_MSGSIZE);

		grn_obj_close(ctx, expression);
		ereport(ERROR,
				(errcode(PGrnRCToPgErrorCode(rc)),
				 errmsg("pgroonga: failed to parse expression: %s",
						message)));
	}

	grn_obj_reinit(ctx, &(buffers->general), GRN_DB_TEXT, 0);
	GRN_TEXT_SET(ctx, &(buffers->general), target, targetSize);
	grn_obj_set_value(ctx,
					  matchSequentialSearchData.textColumn,
					  matchSequentialSearchData.recordID,
					  &(buffers->general),
					  GRN_OBJ_SET);
	GRN_RECORD_SET(ctx, variable, matchSequentialSearchData.recordID);

	result = grn_expr_exec(ctx, expression, 0);
	GRN_OBJ_IS_TRUE(ctx, result, matched);

	grn_obj_close(ctx, expression);

	return matched;
}

/**
 * pgroonga.script_text(target text, script text) : bool
 */
Datum
pgroonga_script_text(PG_FUNCTION_ARGS)
{
	text *target = PG_GETARG_TEXT_PP(0);
	text *script = PG_GETARG_TEXT_PP(1);
	bool matched = false;

	matched = pgroonga_script_raw(VARDATA_ANY(target),
								  VARSIZE_ANY_EXHDR(target),
								  VARDATA_ANY(script),
								  VARSIZE_ANY_EXHDR(script));

	PG_RETURN_BOOL(matched);
}

static grn_bool
pgroonga_prefix_raw(const char *text, unsigned int textSize,
					const char *prefix, unsigned int prefixSize)
{
	grn_bool matched;
	grn_obj targetBuffer;
	grn_obj prefixBuffer;

	GRN_TEXT_INIT(&targetBuffer, GRN_OBJ_DO_SHALLOW_COPY);
	GRN_TEXT_SET(ctx, &targetBuffer, text, textSize);

	GRN_TEXT_INIT(&prefixBuffer, GRN_OBJ_DO_SHALLOW_COPY);
	GRN_TEXT_SET(ctx, &prefixBuffer, prefix, prefixSize);

	matched = grn_operator_exec_prefix(ctx, &targetBuffer, &prefixBuffer);

	GRN_OBJ_FIN(ctx, &targetBuffer);
	GRN_OBJ_FIN(ctx, &prefixBuffer);

	return matched;
}

/**
 * pgroonga.prefix_text(target text, prefix text) : bool
 */
Datum
pgroonga_prefix_text(PG_FUNCTION_ARGS)
{
	text *target = PG_GETARG_TEXT_PP(0);
	text *prefix = PG_GETARG_TEXT_PP(1);
	bool matched = false;

	matched = pgroonga_prefix_raw(VARDATA_ANY(target),
								  VARSIZE_ANY_EXHDR(target),
								  VARDATA_ANY(prefix),
								  VARSIZE_ANY_EXHDR(prefix));

	PG_RETURN_BOOL(matched);
}

static grn_bool
pgroonga_prefix_rk_raw(const char *text, unsigned int textSize,
					   const char *prefix, unsigned int prefixSize)
{
	grn_obj *expression;
	grn_obj *variable;
	grn_bool matched;
	grn_id id;

	GRN_EXPR_CREATE_FOR_QUERY(ctx,
							  prefixRKSequentialSearchData.table,
							  expression,
							  variable);
	if (!expression)
	{
		ereport(ERROR,
				(errcode(ERRCODE_OUT_OF_MEMORY),
				 errmsg("pgroonga: failed to create expression: %s",
						ctx->errbuf)));
	}

	grn_expr_append_obj(ctx, expression,
						grn_ctx_get(ctx, "prefix_rk_search", -1),
						GRN_OP_PUSH, 1);
	grn_expr_append_obj(ctx, expression,
						prefixRKSequentialSearchData.key,
						GRN_OP_GET_VALUE, 1);
	grn_expr_append_const_str(ctx, expression,
							  prefix, prefixSize,
							  GRN_OP_PUSH, 1);
	grn_expr_append_op(ctx, expression, GRN_OP_CALL, 2);

	id = grn_table_add(ctx,
					   prefixRKSequentialSearchData.table,
					   text, textSize, NULL);
	grn_table_select(ctx,
					 prefixRKSequentialSearchData.table,
					 expression,
					 prefixRKSequentialSearchData.resultTable,
					 GRN_OP_OR);
	matched = grn_table_size(ctx, prefixRKSequentialSearchData.resultTable) > 0;
	grn_table_delete(ctx,
					 prefixRKSequentialSearchData.resultTable,
					 &id, sizeof(grn_id));
	grn_table_delete(ctx,
					 prefixRKSequentialSearchData.table,
					 text, textSize);

	grn_obj_close(ctx, expression);

	return matched;
}

/**
 * pgroonga.prefix_rk_text(target text, prefix text) : bool
 */
Datum
pgroonga_prefix_rk_text(PG_FUNCTION_ARGS)
{
	text *target = PG_GETARG_TEXT_PP(0);
	text *prefix = PG_GETARG_TEXT_PP(1);
	bool matched = false;

	matched = pgroonga_prefix_rk_raw(VARDATA_ANY(target),
									 VARSIZE_ANY_EXHDR(target),
									 VARDATA_ANY(prefix),
									 VARSIZE_ANY_EXHDR(prefix));

	PG_RETURN_BOOL(matched);
}

/**
 * pgroonga.match_contain_text(target text, keywords text[]) : bool
 */
Datum
pgroonga_match_contain_text(PG_FUNCTION_ARGS)
{
	text *target = PG_GETARG_TEXT_PP(0);
	ArrayType *keywords = PG_GETARG_ARRAYTYPE_P(1);
	grn_bool matched = GRN_FALSE;
	int i, n;

	n = ARR_DIMS(keywords)[0];
	for (i = 1; i <= n; i++)
	{
		Datum keywordDatum;
		text *keyword;
		bool isNULL;

		keywordDatum = array_ref(keywords, 1, &i, -1, -1, false, 'i', &isNULL);
		if (isNULL)
			continue;

		keyword = DatumGetTextPP(keywordDatum);
		matched = pgroonga_match_term_raw(VARDATA_ANY(target),
										  VARSIZE_ANY_EXHDR(target),
										  VARDATA_ANY(keyword),
										  VARSIZE_ANY_EXHDR(keyword));
		if (matched)
			break;
	}

	PG_RETURN_BOOL(matched);
}

/**
 * pgroonga.query_contain_text(target text, queries text[]) : bool
 */
Datum
pgroonga_query_contain_text(PG_FUNCTION_ARGS)
{
	text *target = PG_GETARG_TEXT_PP(0);
	ArrayType *queries = PG_GETARG_ARRAYTYPE_P(1);
	grn_bool matched = GRN_FALSE;
	int i, n;

	n = ARR_DIMS(queries)[0];
	for (i = 1; i <= n; i++)
	{
		Datum queryDatum;
		text *query;
		bool isNULL;

		queryDatum = array_ref(queries, 1, &i, -1, -1, false, 'i', &isNULL);
		if (isNULL)
			continue;

		query = DatumGetTextPP(queryDatum);
		matched = pgroonga_match_query_raw(VARDATA_ANY(target),
										   VARSIZE_ANY_EXHDR(target),
										   VARDATA_ANY(query),
										   VARSIZE_ANY_EXHDR(query));
		if (matched)
			break;
	}

	PG_RETURN_BOOL(matched);
}

/**
 * pgroonga.prefix_contain_text_array(targets text[], prefix text) : bool
 */
Datum
pgroonga_prefix_contain_text_array(PG_FUNCTION_ARGS)
{
	ArrayType *targets = PG_GETARG_ARRAYTYPE_P(0);
	text *prefix = PG_GETARG_TEXT_PP(1);
	bool matched = false;

	int i, n;

	n = ARR_DIMS(targets)[0];
	for (i = 1; i <= n; i++)
	{
		Datum targetDatum;
		text *target;
		bool isNULL;

		targetDatum = array_ref(targets, 1, &i, -1, -1, false, 'i', &isNULL);
		if (isNULL)
			continue;

		target = DatumGetTextPP(targetDatum);
		matched = pgroonga_prefix_raw(VARDATA_ANY(target),
									  VARSIZE_ANY_EXHDR(target),
									  VARDATA_ANY(prefix),
									  VARSIZE_ANY_EXHDR(prefix));
		if (matched)
		{
			break;
		}
	}

	PG_RETURN_BOOL(matched);
}

/**
 * pgroonga.prefix_rk_contain_text_array(targets text[], prefix text) : bool
 */
Datum
pgroonga_prefix_rk_contain_text_array(PG_FUNCTION_ARGS)
{
	ArrayType *targets = PG_GETARG_ARRAYTYPE_P(0);
	text *prefix = PG_GETARG_TEXT_PP(1);
	bool matched = false;
	int i, n;

	n = ARR_DIMS(targets)[0];
	for (i = 1; i <= n; i++)
	{
		Datum targetDatum;
		text *target;
		bool isNULL;

		targetDatum = array_ref(targets, 1, &i, -1, -1, false, 'i', &isNULL);
		if (isNULL)
			continue;

		target = DatumGetTextPP(targetDatum);
		matched = pgroonga_prefix_rk_raw(VARDATA_ANY(target),
										 VARSIZE_ANY_EXHDR(target),
										 VARDATA_ANY(prefix),
										 VARSIZE_ANY_EXHDR(prefix));
		if (matched)
		{
			break;
		}
	}

	PG_RETURN_BOOL(matched);
}

static bool
PGrnNeedMaxRecordSizeUpdate(Relation index)
{
	TupleDesc desc = RelationGetDescr(index);
	unsigned int nVarCharColumns = 0;
	unsigned int i;

	for (i = 0; i < desc->natts; i++)
	{
		Form_pg_attribute attribute;

		attribute = desc->attrs[i];
		switch (attribute->atttypid)
		{
		case VARCHAROID:
			nVarCharColumns++;
			break;
		case TEXTOID:
		case VARCHARARRAYOID:
		case TEXTARRAYOID:
			return true;
			break;
		default:
			break;
		}
	}

	return nVarCharColumns >= 2;
}

#define PGRN_INDEX_ONLY_SCAN_THRESHOLD_SIZE (INDEX_SIZE_MASK * 0.9)

static void
PGrnUpdateMaxRecordSize(Relation index,
						uint32_t recordSize)
{
	uint32_t currentMaxRecordSize;

	if (recordSize < PGRN_INDEX_ONLY_SCAN_THRESHOLD_SIZE)
		return;

	currentMaxRecordSize = PGrnIndexStatusGetMaxRecordSize(index);
	if (recordSize < currentMaxRecordSize)
		return;

	PGrnIndexStatusSetMaxRecordSize(index, recordSize);
}

static uint32_t
PGrnInsert(Relation index,
		   grn_obj *sourcesTable,
		   grn_obj *sourcesCtidColumn,
		   Datum *values,
		   bool *isnull,
		   ItemPointer ht_ctid)
{
	TupleDesc desc = RelationGetDescr(index);
	grn_id id;
	PGrnWALData *walData;
	unsigned int i;
	uint32_t recordSize = 0;

	if (desc->natts == 1 && PGrnAttributeIsJSONB(desc->attrs[0]->atttypid))
	{
		return PGrnJSONBInsert(index,
							   sourcesTable,
							   sourcesCtidColumn,
							   values,
							   isnull,
							   CtidToUInt64(ht_ctid));
	}

	id = grn_table_add(ctx, sourcesTable, NULL, 0, NULL);

	walData = PGrnWALStart(index);
	PGrnWALInsertStart(walData, NULL, desc->natts + 1);

	GRN_UINT64_SET(ctx, &(buffers->ctid), CtidToUInt64(ht_ctid));
	grn_obj_set_value(ctx, sourcesCtidColumn, id, &(buffers->ctid), GRN_OBJ_SET);
	PGrnWALInsertColumn(walData, sourcesCtidColumn, &(buffers->ctid));

	for (i = 0; i < desc->natts; i++)
	{
		grn_obj *dataColumn;
		Form_pg_attribute attribute = desc->attrs[i];
		NameData *name;
		grn_id domain;
		unsigned char flags;
		grn_obj *buffer;

		name = &(attribute->attname);
		if (isnull[i])
			continue;

		dataColumn = PGrnLookupColumn(sourcesTable, name->data, ERROR);
		buffer = &(buffers->general);
		domain = PGrnGetType(index, i, &flags);
		grn_obj_reinit(ctx, buffer, domain, flags);
		PGrnConvertFromData(values[i], attribute->atttypid, buffer);
		grn_obj_set_value(ctx, dataColumn, id, buffer, GRN_OBJ_SET);
		recordSize += GRN_BULK_VSIZE(buffer);
		PGrnWALInsertColumn(walData, dataColumn, buffer);
		grn_obj_unlink(ctx, dataColumn);
		if (!PGrnCheck("failed to set column value")) {
			continue;
		}
	}

	PGrnWALInsertFinish(walData);
	PGrnWALFinish(walData);

	return recordSize;
}

static bool
pgroonga_insert_raw(Relation index,
					Datum *values,
					bool *isnull,
					ItemPointer ctid,
					Relation heap,
					IndexUniqueCheck checkUnique)
{
	grn_obj *sourcesTable;
	grn_obj *sourcesCtidColumn;
	uint32_t recordSize;

	sourcesTable = PGrnLookupSourcesTable(index, ERROR);
	sourcesCtidColumn = PGrnLookupSourcesCtidColumn(index, ERROR);
	recordSize = PGrnInsert(index,
							sourcesTable,
							sourcesCtidColumn,
							values,
							isnull,
							ctid);
	if (PGrnNeedMaxRecordSizeUpdate(index))
		PGrnUpdateMaxRecordSize(index, recordSize);
	grn_db_touch(ctx, grn_ctx_db(ctx));

	return false;
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
	ItemPointer ctid = (ItemPointer) PG_GETARG_POINTER(3);
	Relation heap = (Relation) PG_GETARG_POINTER(4);
	IndexUniqueCheck uniqueCheck = PG_GETARG_INT32(5);
	bool isUnique;

	isUnique = pgroonga_insert_raw(index,
								   values,
								   isnull,
								   ctid,
								   heap,
								   uniqueCheck);

	PG_RETURN_BOOL(isUnique);
}

#ifdef PGRN_SUPPORT_SCORE
static void
PGrnPrimaryKeyColumnsFin(slist_head *columns)
{
	while (!slist_is_empty(columns))
	{
		slist_node *current;
		PGrnPrimaryKeyColumn *column;

		current = slist_pop_head_node(columns);
		column = slist_container(PGrnPrimaryKeyColumn, node, current);
		pfree(column);
	}
}

static void
PGrnPrimaryKeyColumnsInit(slist_head *columns,
						  PGrnScanOpaque so)
{
	Relation table;
	List *indexOIDList;
	ListCell *cell;

	table = RelationIdGetRelation(so->dataTableID);
	indexOIDList = RelationGetIndexList(table);
	foreach(cell, indexOIDList)
	{
		Oid indexOID = lfirst_oid(cell);
		Relation primaryKeyIndex;
		int i;

		primaryKeyIndex = index_open(indexOID, NoLock);
		if (!primaryKeyIndex->rd_index->indisprimary) {
			index_close(primaryKeyIndex, NoLock);
			continue;
		}

		for (i = 0; i < primaryKeyIndex->rd_index->indnatts; i++)
		{
			Oid primaryKeyNumber;
			int j;
			bool havePrimaryKey = false;

			primaryKeyNumber = primaryKeyIndex->rd_index->indkey.values[i];

			for (j = 0; j < so->index->rd_index->indnatts; j++)
			{
				TupleDesc desc;
				const char *columnName;
				PGrnPrimaryKeyColumn *primaryKeyColumn;

				if (so->index->rd_index->indkey.values[j] != primaryKeyNumber)
					continue;

				primaryKeyColumn =
					(PGrnPrimaryKeyColumn *) palloc(sizeof(PGrnPrimaryKeyColumn));

				desc = RelationGetDescr(table);
				columnName = so->index->rd_att->attrs[j]->attname.data;

				primaryKeyColumn->number = primaryKeyNumber;
				primaryKeyColumn->type =
					desc->attrs[primaryKeyNumber - 1]->atttypid;
				primaryKeyColumn->domain = PGrnGetType(primaryKeyIndex,
													   primaryKeyNumber - 1,
													   &(primaryKeyColumn->flags));
				primaryKeyColumn->column = grn_obj_column(ctx,
														  so->sourcesTable,
														  columnName,
														  strlen(columnName));
				slist_push_head(columns, &(primaryKeyColumn->node));
				havePrimaryKey = true;
				break;
			}

			if (!havePrimaryKey)
			{
				PGrnPrimaryKeyColumnsFin(columns);
				break;
			}
		}

		index_close(primaryKeyIndex, NoLock);
		break;
	}
	list_free(indexOIDList);
	RelationClose(table);
}

static void
PGrnScanOpaqueInitPrimaryKeyColumns(PGrnScanOpaque so)
{
	slist_init(&(so->primaryKeyColumns));
	PGrnPrimaryKeyColumnsInit(&(so->primaryKeyColumns), so);
}
#endif

static void
PGrnScanOpaqueInit(PGrnScanOpaque so, Relation index)
{
	so->index = index;
	so->dataTableID = index->rd_index->indrelid;
	so->sourcesTable = PGrnLookupSourcesTable(index, ERROR);
	so->sourcesCtidColumn = PGrnLookupSourcesCtidColumn(index, ERROR);
	GRN_VOID_INIT(&(so->minBorderValue));
	GRN_VOID_INIT(&(so->maxBorderValue));
	so->searched = NULL;
	so->sorted = NULL;
	so->targetTable = NULL;
	so->indexCursor = NULL;
	so->tableCursor = NULL;
	so->ctidAccessor = NULL;
	so->scoreAccessor = NULL;
	so->currentID = GRN_ID_NIL;

#ifdef PGRN_SUPPORT_SCORE
	slist_push_head(&PGrnScanOpaques, &(so->node));
	PGrnScanOpaqueInitPrimaryKeyColumns(so);
#endif
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
	if (so->ctidAccessor)
	{
		grn_obj_unlink(ctx, so->ctidAccessor);
		so->ctidAccessor = NULL;
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
		grn_obj_close(ctx, so->sorted);
		so->sorted = NULL;
	}
	if (so->searched)
	{
		grn_obj_close(ctx, so->searched);
		so->searched = NULL;
	}
}

static void
PGrnScanOpaqueFin(PGrnScanOpaque so)
{
#ifdef PGRN_SUPPORT_SCORE
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

	PGrnPrimaryKeyColumnsFin(&(so->primaryKeyColumns));
#endif

	PGrnScanOpaqueReinit(so);
}

static IndexScanDesc
pgroonga_beginscan_raw(Relation index,
					   int nKeys,
#ifdef PGRN_IS_GREENPLUM
					   ScanKey key
#else
					   int nOrderBys
#endif
	)
{
	IndexScanDesc scan;
	PGrnScanOpaque so;

#ifdef PGRN_IS_GREENPLUM
	scan = RelationGetIndexScan(index, nKeys, key);
#else
	scan = RelationGetIndexScan(index, nKeys, nOrderBys);
#endif

	so = (PGrnScanOpaque) palloc(sizeof(PGrnScanOpaqueData));
	PGrnScanOpaqueInit(so, index);

	scan->opaque = so;

	return scan;
}

/**
 * pgroonga.beginscan() -- ambeginscan
 */
Datum
pgroonga_beginscan(PG_FUNCTION_ARGS)
{
	Relation index = (Relation) PG_GETARG_POINTER(0);
	int nKeys = PG_GETARG_INT32(1);
#ifdef PGRN_IS_GREENPLUM
	ScanKey key = (ScanKey) PG_GETARG_POINTER(2);
#else
	int nOrderBys = PG_GETARG_INT32(2);
#endif
	IndexScanDesc scan;

	scan = pgroonga_beginscan_raw(index,
								  nKeys,
#ifdef PGRN_IS_GREENPLUM
								  key
#else
								  nOrderBys
#endif
		);

	PG_RETURN_POINTER(scan);
}

static bool
PGrnSearchIsInCondition(ScanKey key)
{
	return (key->sk_flags & SK_SEARCHARRAY &&
			key->sk_strategy == PGrnEqualStrategyNumber);
}

static bool
PGrnSearchBuildConditionIn(PGrnSearchData *data,
						   ScanKey key,
						   grn_obj *targetColumn,
						   Form_pg_attribute attribute)
{
	grn_id domain;
	unsigned char flags = 0;
	ArrayType *values;
	int i, n;

	domain = PGrnPGTypeToGrnType(attribute->atttypid, &flags);
	grn_obj_reinit(ctx, &(buffers->general), domain, flags);
	values = DatumGetArrayTypeP(key->sk_argument);
	n = ARR_DIMS(values)[0];

	grn_expr_append_obj(ctx, data->expression,
						PGrnLookup("in_values", ERROR),
						GRN_OP_PUSH,
						1);
	PGrnCheck("IN: failed to push in_values()");
	grn_expr_append_obj(ctx, data->expression,
						targetColumn,
						GRN_OP_PUSH,
						1);
	PGrnCheck("IN: failed to push target column");
	grn_expr_append_op(ctx, data->expression, GRN_OP_GET_VALUE, 1);
	PGrnCheck("IN: failed to push GET_VALUE");

	for (i = 1; i <= n; i++)
	{
		Datum valueDatum;
		bool isNULL;

		valueDatum = array_ref(values, 1, &i, -1,
							   attribute->attlen,
							   attribute->attbyval,
							   attribute->attalign,
							   &isNULL);
		if (isNULL)
			return false;

		PGrnConvertFromData(valueDatum,
							attribute->atttypid,
							&(buffers->general));
		grn_expr_append_const(ctx,
							  data->expression,
							  &(buffers->general),
							  GRN_OP_PUSH,
							  1);
		PGrnCheck("IN: failed to push a value");
	}

	grn_expr_append_op(ctx, data->expression, GRN_OP_CALL, 2 + (n - 1));
	PGrnCheck("IN: failed to push CALL");

	return true;
}

static void
PGrnSearchBuildConditionLikeMatchFlush(grn_obj *expression,
									   grn_obj *targetColumn,
									   grn_obj *keyword,
									   int *nKeywords)
{
	if (GRN_TEXT_LEN(keyword) == 0)
		return;

	grn_expr_append_obj(ctx, expression, targetColumn, GRN_OP_PUSH, 1);
	grn_expr_append_op(ctx, expression, GRN_OP_GET_VALUE, 1);
	grn_expr_append_const_str(ctx, expression,
							  GRN_TEXT_VALUE(keyword),
							  GRN_TEXT_LEN(keyword),
							  GRN_OP_PUSH, 1);
	grn_expr_append_op(ctx, expression, GRN_OP_MATCH, 2);
	if (*nKeywords > 0)
			grn_expr_append_op(ctx, expression, GRN_OP_OR, 2);
	(*nKeywords)++;

	GRN_BULK_REWIND(keyword);
}

static void
PGrnSearchBuildConditionLikeMatch(PGrnSearchData *data,
								  grn_obj *targetColumn,
								  grn_obj *query)
{
	grn_obj *expression;
	const char *queryRaw;
	size_t i, querySize;
	int nKeywords = 0;

	expression = data->expression;
	queryRaw = GRN_TEXT_VALUE(query);
	querySize = GRN_TEXT_LEN(query);

	if (querySize == 0)
	{
		data->isEmptyCondition = true;
		return;
	}

	GRN_BULK_REWIND(&(buffers->keyword));
	for (i = 0; i < querySize; i++)
	{
		switch (queryRaw[i])
		{
		case '\\':
			if (i == querySize)
			{
				GRN_TEXT_PUTC(ctx, &(buffers->keyword), '\\');
			}
			else
			{
				GRN_TEXT_PUTC(ctx, &(buffers->keyword), queryRaw[i + 1]);
				i++;
			}
			break;
		case '%':
		case '_':
			PGrnSearchBuildConditionLikeMatchFlush(expression,
												   targetColumn,
												   &(buffers->keyword),
												   &nKeywords);
			break;
		default:
			GRN_TEXT_PUTC(ctx, &(buffers->keyword), queryRaw[i]);
			break;
		}
	}

	PGrnSearchBuildConditionLikeMatchFlush(expression,
										   targetColumn,
										   &(buffers->keyword),
										   &nKeywords);
	if (nKeywords == 0)
	{
		grn_expr_append_obj(ctx, expression,
							grn_ctx_get(ctx, "all_records", -1),
							GRN_OP_PUSH, 1);
		grn_expr_append_op(ctx, expression, GRN_OP_CALL, 0);
	}
}

static void
PGrnSearchBuildConditionLikeRegexp(PGrnSearchData *data,
								   grn_obj *targetColumn,
								   grn_obj *query)
{
	grn_obj *expression;
	const char *queryRaw;
	const char *queryRawEnd;
	const char *queryRawCurrent;
	size_t querySize;
	int characterSize;
	bool escaping = false;
	bool lastIsPercent = false;

	expression = data->expression;
	queryRaw = GRN_TEXT_VALUE(query);
	querySize = GRN_TEXT_LEN(query);
	queryRawEnd = queryRaw + querySize;

	GRN_BULK_REWIND(&(buffers->pattern));
	if (queryRaw[0] != '%')
		GRN_TEXT_PUTS(ctx, &(buffers->pattern), "\\A");

	queryRawCurrent = queryRaw;
	while ((characterSize = grn_charlen(ctx, queryRawCurrent, queryRawEnd)) > 0)
	{
		const char *current = queryRawCurrent;
		bool needToAddCharacter = true;

		queryRawCurrent += characterSize;

		if (!escaping && characterSize == 1)
		{
			switch (current[0])
			{
			case '%':
				if (queryRaw == current)
				{
					/* do nothing */
				}
				else if (queryRawCurrent == queryRawEnd)
				{
					lastIsPercent = true;
				}
				else
				{
					GRN_TEXT_PUTS(ctx, &(buffers->pattern), ".*");
				}
				needToAddCharacter = false;
				break;
			case '_':
				GRN_TEXT_PUTC(ctx, &(buffers->pattern), '.');
				needToAddCharacter = false;
				break;
			case '\\':
				escaping = true;
				needToAddCharacter = false;
				break;
			default:
				break;
			}

			if (!needToAddCharacter)
				continue;
		}

		if (characterSize == 1)
		{
			switch (current[0])
			{
			case '\\':
			case '|':
			case '(':
			case ')':
			case '[':
			case ']':
			case '.':
			case '*':
			case '+':
			case '?':
			case '{':
			case '}':
			case '^':
			case '$':
				GRN_TEXT_PUTC(ctx, &(buffers->pattern), '\\');
				GRN_TEXT_PUTC(ctx, &(buffers->pattern), current[0]);
				break;
			default:
				GRN_TEXT_PUTC(ctx, &(buffers->pattern), current[0]);
				break;
			}
		}
		else
		{
			GRN_TEXT_PUT(ctx, &(buffers->pattern), current, characterSize);
		}
		escaping = false;
	}

	if (queryRawCurrent != queryRawEnd)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("invalid encoding character exist: <%.*s>",
						(int) querySize, queryRaw)));
	}

	if (!lastIsPercent)
		GRN_TEXT_PUTS(ctx, &(buffers->pattern), "\\z");

	grn_expr_append_obj(ctx, expression, targetColumn, GRN_OP_PUSH, 1);
	grn_expr_append_op(ctx, expression, GRN_OP_GET_VALUE, 1);
	grn_expr_append_const_str(ctx, expression,
							  GRN_TEXT_VALUE(&(buffers->pattern)),
							  GRN_TEXT_LEN(&(buffers->pattern)),
							  GRN_OP_PUSH, 1);
	grn_expr_append_op(ctx, expression, GRN_OP_REGEXP, 2);
}

static void
PGrnSearchBuildConditionQuery(PGrnSearchData *data,
							  grn_obj *targetColumn,
							  const char *query,
							  unsigned int querySize)
{
	grn_rc rc;
	grn_obj *matchTarget, *matchTargetVariable;
	grn_expr_flags flags = GRN_EXPR_SYNTAX_QUERY | GRN_EXPR_ALLOW_LEADING_NOT;

	GRN_EXPR_CREATE_FOR_QUERY(ctx, data->sourcesTable,
							  matchTarget, matchTargetVariable);
	GRN_PTR_PUT(ctx, &(data->matchTargets), matchTarget);
	grn_expr_append_obj(ctx, matchTarget, targetColumn, GRN_OP_PUSH, 1);

	rc = grn_expr_parse(ctx, data->expression,
						query, querySize,
						matchTarget, GRN_OP_MATCH, GRN_OP_AND,
						flags);
	if (rc != GRN_SUCCESS)
	{
		ereport(ERROR,
				(errcode(PGrnRCToPgErrorCode(rc)),
				 errmsg("pgroonga: failed to parse expression: %s",
						ctx->errbuf)));
	}
}

static void
PGrnSearchBuildConditionPrefixRK(PGrnSearchData *data,
								 grn_obj *targetColumn,
								 const char *prefix,
								 unsigned int prefixSize)
{
	grn_obj subFilterScript;

	GRN_TEXT_INIT(&subFilterScript, 0);
	GRN_TEXT_PUTS(ctx, &subFilterScript, "prefix_rk_search(_key, ");
	grn_text_esc(ctx, &subFilterScript, prefix, prefixSize);
	GRN_TEXT_PUTS(ctx, &subFilterScript, ")");

	grn_expr_append_obj(ctx, data->expression,
						grn_ctx_get(ctx, "sub_filter", -1),
						GRN_OP_PUSH, 1);
	grn_expr_append_obj(ctx, data->expression,
						targetColumn,
						GRN_OP_GET_VALUE, 1);
	grn_expr_append_const_str(ctx, data->expression,
							  GRN_TEXT_VALUE(&subFilterScript),
							  GRN_TEXT_LEN(&subFilterScript),
							  GRN_OP_PUSH, 1);
	grn_expr_append_op(ctx, data->expression, GRN_OP_CALL, 2);

	GRN_OBJ_FIN(ctx, &subFilterScript);
}

static void
PGrnSearchBuildConditionScript(PGrnSearchData *data,
							   grn_obj *targetColumn,
							   const char *script,
							   unsigned int scriptSize)
{
	grn_rc rc;
	grn_obj *matchTarget, *matchTargetVariable;
	grn_expr_flags flags = GRN_EXPR_SYNTAX_SCRIPT;

	GRN_EXPR_CREATE_FOR_QUERY(ctx, data->sourcesTable,
							  matchTarget, matchTargetVariable);
	GRN_PTR_PUT(ctx, &(data->matchTargets), matchTarget);
	grn_expr_append_obj(ctx, matchTarget, targetColumn, GRN_OP_PUSH, 1);

	rc = grn_expr_parse(ctx, data->expression,
						script, scriptSize,
						matchTarget, GRN_OP_MATCH, GRN_OP_AND,
						flags);
	if (rc != GRN_SUCCESS)
	{
		ereport(ERROR,
				(errcode(PGrnRCToPgErrorCode(rc)),
				 errmsg("pgroonga: failed to parse expression: %s",
						ctx->errbuf)));
	}
}

static void
PGrnSearchBuildConditionBinaryOperation(PGrnSearchData *data,
										grn_obj *targetColumn,
										grn_obj *value,
										grn_operator operator)
{
	grn_expr_append_obj(ctx, data->expression,
						targetColumn, GRN_OP_PUSH, 1);
	grn_expr_append_op(ctx, data->expression, GRN_OP_GET_VALUE, 1);
	grn_expr_append_const(ctx, data->expression,
						  value, GRN_OP_PUSH, 1);
	grn_expr_append_op(ctx, data->expression, operator, 2);
}

static bool
PGrnSearchBuildCondition(Relation index,
						 ScanKey key,
						 PGrnSearchData *data)
{
	TupleDesc desc;
	Form_pg_attribute attribute;
	const char *targetColumnName;
	grn_obj *targetColumn;
	grn_operator operator = GRN_OP_NOP;
	Oid valueTypeID;

	/* NULL key is not supported */
	if (key->sk_flags & SK_ISNULL)
		return false;

	desc = RelationGetDescr(index);
	attribute = desc->attrs[key->sk_attno - 1];

	targetColumnName = attribute->attname.data;
	targetColumn = PGrnLookupColumn(data->sourcesTable, targetColumnName, ERROR);
	GRN_PTR_PUT(ctx, &(data->targetColumns), targetColumn);

	if (PGrnSearchIsInCondition(key))
		return PGrnSearchBuildConditionIn(data, key, targetColumn, attribute);

	if (PGrnAttributeIsJSONB(attribute->atttypid))
		return PGrnJSONBBuildSearchCondition(data, key, targetColumn);

	valueTypeID = attribute->atttypid;
	switch (valueTypeID)
	{
	case VARCHARARRAYOID:
		valueTypeID = VARCHAROID;
		break;
	case TEXTARRAYOID:
		valueTypeID = TEXTOID;
		break;
	}

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
	case PGrnILikeStrategyNumber:
		break;
	case PGrnMatchStrategyNumber:
	case PGrnMatchStrategyV2Number:
		operator = GRN_OP_MATCH;
		break;
	case PGrnQueryStrategyNumber:
	case PGrnQueryStrategyV2Number:
		break;
	case PGrnSimilarStrategyV2Number:
		operator = GRN_OP_SIMILAR;
		break;
	case PGrnScriptStrategyV2Number:
		break;
	case PGrnPrefixStrategyV2Number:
	case PGrnPrefixContainStrategyV2Number:
		operator = GRN_OP_PREFIX;
		break;
	case PGrnPrefixRKStrategyV2Number:
	case PGrnPrefixRKContainStrategyV2Number:
		break;
	case PGrnRegexpStrategyNumber:
		operator = GRN_OP_REGEXP;
		break;
	case PGrnQueryContainStrategyV2Number:
	case PGrnMatchContainStrategyV2Number:
		switch (attribute->atttypid)
		{
		case TEXTOID:
			valueTypeID = TEXTARRAYOID;
			break;
		}
		break;
	default:
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("unexpected strategy number: %d",
						key->sk_strategy)));
		break;
	}

	{
		grn_id domain;
		unsigned char flags = 0;
		domain = PGrnPGTypeToGrnType(valueTypeID, &flags);
		grn_obj_reinit(ctx, &(buffers->general), domain, flags);
		PGrnConvertFromData(key->sk_argument, valueTypeID, &(buffers->general));
	}

	switch (key->sk_strategy)
	{
	case PGrnLikeStrategyNumber:
		if (PGrnIsForRegexpSearchIndex(index, key->sk_attno - 1))
			PGrnSearchBuildConditionLikeRegexp(data, targetColumn, &(buffers->general));
		else
			PGrnSearchBuildConditionLikeMatch(data, targetColumn, &(buffers->general));
		break;
	case PGrnILikeStrategyNumber:
		PGrnSearchBuildConditionLikeMatch(data, targetColumn, &(buffers->general));
		break;
	case PGrnQueryStrategyNumber:
	case PGrnQueryStrategyV2Number:
		PGrnSearchBuildConditionQuery(data,
									  targetColumn,
									  GRN_TEXT_VALUE(&(buffers->general)),
									  GRN_TEXT_LEN(&(buffers->general)));
		break;
	case PGrnScriptStrategyV2Number:
		PGrnSearchBuildConditionScript(data,
									   targetColumn,
									   GRN_TEXT_VALUE(&(buffers->general)),
									   GRN_TEXT_LEN(&(buffers->general)));
		break;
	case PGrnPrefixRKStrategyV2Number:
	case PGrnPrefixRKContainStrategyV2Number:
		PGrnSearchBuildConditionPrefixRK(data,
										 targetColumn,
										 GRN_TEXT_VALUE(&(buffers->general)),
										 GRN_TEXT_LEN(&(buffers->general)));
		break;
	case PGrnQueryContainStrategyV2Number:
	{
		grn_obj *queries = &(buffers->general);
		unsigned int i, n;

		n = grn_vector_size(ctx, queries);
		for (i = 0; i < n; i++)
		{
			const char *query;
			unsigned int querySize;

			querySize = grn_vector_get_element(ctx, queries, i,
												&query, NULL, NULL);
			PGrnSearchBuildConditionQuery(data,
										  targetColumn,
										  query,
										  querySize);
			if (i > 0)
				grn_expr_append_op(ctx, data->expression, GRN_OP_OR, 2);
		}
		break;
	}
	case PGrnMatchContainStrategyV2Number:
	{
		grn_obj *keywords = &(buffers->general);
		grn_obj keywordBuffer;
		unsigned int i, n;

		GRN_TEXT_INIT(&keywordBuffer, GRN_OBJ_DO_SHALLOW_COPY);
		n = grn_vector_size(ctx, keywords);
		for (i = 0; i < n; i++)
		{
			const char *keyword;
			unsigned int keywordSize;

			keywordSize = grn_vector_get_element(ctx, keywords, i,
												&keyword, NULL, NULL);
			GRN_TEXT_SET(ctx, &keywordBuffer, keyword, keywordSize);
			PGrnSearchBuildConditionBinaryOperation(data,
													targetColumn,
													&keywordBuffer,
													GRN_OP_MATCH);
			if (i > 0)
				grn_expr_append_op(ctx, data->expression, GRN_OP_OR, 2);
		}
		GRN_OBJ_FIN(ctx, &keywordBuffer);
		break;
	}
	default:
		PGrnSearchBuildConditionBinaryOperation(data,
												targetColumn,
												&(buffers->general),
												operator);
		break;
	}

	return true;
}

static void
PGrnSearchBuildConditions(IndexScanDesc scan,
						  PGrnScanOpaque so,
						  PGrnSearchData *data)
{
	int i, nExpressions = 0;

	for (i = 0; i < scan->numberOfKeys; i++)
	{
		Relation index = scan->indexRelation;
		ScanKey key = &(scan->keyData[i]);

		if (!PGrnSearchBuildCondition(index, key, data))
			continue;

		if (data->isEmptyCondition)
			return;

		if (nExpressions > 0)
			grn_expr_append_op(ctx, data->expression, GRN_OP_AND, 2);
		nExpressions++;
	}
}

static void
PGrnSearchDataInit(PGrnSearchData *data, grn_obj *sourcesTable)
{
	data->sourcesTable = sourcesTable;
	GRN_PTR_INIT(&(data->matchTargets), GRN_OBJ_VECTOR, GRN_ID_NIL);
	GRN_PTR_INIT(&(data->targetColumns), GRN_OBJ_VECTOR, GRN_ID_NIL);
	GRN_UINT32_INIT(&(data->sectionID), 0);

	GRN_EXPR_CREATE_FOR_QUERY(ctx, sourcesTable,
							  data->expression, data->expressionVariable);
	data->isEmptyCondition = false;
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

	PGrnSearchDataInit(&data, so->sourcesTable);
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
									so->sourcesTable, 0);
	if (!data.isEmptyCondition)
	{
		grn_table_select(ctx,
						 so->sourcesTable,
						 data.expression,
						 so->searched,
						 GRN_OP_OR);
	}
	PGrnSearchDataFree(&data);
}

static void
PGrnSort(IndexScanDesc scan)
{
	PGrnScanOpaque so = (PGrnScanOpaque) scan->opaque;
	ScanKey key;
	TupleDesc desc;
	Form_pg_attribute attribute;
	const char *targetColumnName;
	grn_table_sort_key sort_key;

	if (!so->searched)
		return;

	if (scan->numberOfKeys != 1)
		return;

	key = &(scan->keyData[0]);
	if (!PGrnSearchIsInCondition(key))
		return;

	so->sorted = grn_table_create(ctx, NULL, 0, NULL,
								  GRN_OBJ_TABLE_NO_KEY,
								  NULL, so->searched);

	desc = RelationGetDescr(scan->indexRelation);
	attribute = desc->attrs[key->sk_attno - 1];
	targetColumnName = attribute->attname.data;
	sort_key.key = grn_obj_column(ctx, so->searched,
								  targetColumnName,
								  strlen(targetColumnName));

	sort_key.flags = GRN_TABLE_SORT_ASC;
	sort_key.offset = 0;
	grn_table_sort(ctx,
				   so->searched,
				   0,
				   -1,
				   so->sorted,
				   &sort_key,
				   1);
	grn_obj_close(ctx, sort_key.key);
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
		table = so->sourcesTable;

	if (dir == BackwardScanDirection)
		flags |= GRN_CURSOR_DESCENDING;
	else
		flags |= GRN_CURSOR_ASCENDING;

	so->tableCursor = grn_table_cursor_open(ctx, table,
											NULL, 0, NULL, 0,
											offset, limit, flags);
	so->ctidAccessor = grn_obj_column(ctx, table,
									  PGrnSourcesCtidColumnName,
									  PGrnSourcesCtidColumnNameLength);
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
	TupleDesc desc;
	PGrnScanOpaque so = (PGrnScanOpaque) scan->opaque;
	grn_obj *minBorderValue;
	grn_obj *maxBorderValue;
	int i;

	desc = RelationGetDescr(index);

	minBorderValue = &(so->minBorderValue);
	maxBorderValue = &(so->maxBorderValue);
	for (i = 0; i < scan->numberOfKeys; i++)
	{
		ScanKey key = &(scan->keyData[i]);
		AttrNumber attrNumber;
		Form_pg_attribute attribute;
		grn_id domain;

		attrNumber = key->sk_attno - 1;
		attribute = desc->attrs[attrNumber];

		domain = PGrnGetType(index, attrNumber, NULL);
		switch (key->sk_strategy)
		{
		case PGrnLessStrategyNumber:
		case PGrnLessEqualStrategyNumber:
			if (maxBorderValue->header.type != GRN_DB_VOID)
			{
				grn_obj_reinit(ctx, &(buffers->general), domain, 0);
				PGrnConvertFromData(key->sk_argument,
									attribute->atttypid,
									&(buffers->general));
				if (!PGrnIsMeaningfullMaxBorderValue(maxBorderValue,
													 &(buffers->general),
													 *flags,
													 key->sk_strategy))
				{
					continue;
				}
			}
			grn_obj_reinit(ctx, maxBorderValue, domain, 0);
			PGrnConvertFromData(key->sk_argument,
								attribute->atttypid,
								maxBorderValue);
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
				grn_obj_reinit(ctx, &(buffers->general), domain, 0);
				PGrnConvertFromData(key->sk_argument,
									attribute->atttypid,
									&(buffers->general));
				if (!PGrnIsMeaningfullMinBorderValue(minBorderValue,
													 &(buffers->general),
													 *flags,
													 key->sk_strategy))
				{
					continue;
				}
			}
			grn_obj_reinit(ctx, minBorderValue, domain, 0);
			PGrnConvertFromData(key->sk_argument,
								attribute->atttypid,
								minBorderValue);
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
	so->ctidAccessor = grn_obj_column(ctx, so->sourcesTable,
									  PGrnSourcesCtidColumnName,
									  PGrnSourcesCtidColumnNameLength);
}

static bool
PGrnIsRangeSearchable(IndexScanDesc scan)
{
	int i;
	AttrNumber previousAttrNumber = InvalidAttrNumber;

	if (scan->numberOfKeys == 0)
	{
		grn_obj *indexColumn;
		grn_obj *lexicon;
		grn_obj *tokenizer;

		indexColumn = PGrnLookupIndexColumn(scan->indexRelation, 0, ERROR);
		lexicon = grn_column_table(ctx, indexColumn);
		tokenizer = grn_obj_get_info(ctx, lexicon, GRN_INFO_DEFAULT_TOKENIZER,
									 NULL);
		if (tokenizer)
		{
			return false;
		}
	}

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
PGrnEnsureCursorOpened(IndexScanDesc scan,
					   ScanDirection dir,
					   bool needSort)
{
	PGrnScanOpaque so = (PGrnScanOpaque) scan->opaque;

#ifdef PGRN_SUPPORT_RECHECK_PER_SCAN
	scan->xs_recheck = false;

	{
		int i;
		for (i = 0; i < scan->numberOfKeys; i++)
		{
			ScanKey key = &(scan->keyData[i]);
			if (key->sk_strategy == PGrnLikeStrategyNumber ||
				key->sk_strategy == PGrnILikeStrategyNumber)
			{
				scan->xs_recheck = true;
				break;
			}
		}
	}
#endif

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
		if (needSort)
			PGrnSort(scan);
		PGrnOpenTableCursor(scan, dir);
	}
}

static grn_id
PGrnScanOpaqueResolveID(PGrnScanOpaque so)
{
	grn_id recordID = so->currentID;

	if (so->sorted)
	{
		GRN_BULK_REWIND(&(buffers->general));
		grn_obj_get_value(ctx, so->sorted, recordID, &(buffers->general));
		recordID = GRN_RECORD_VALUE(&(buffers->general));
	}
	if (so->searched)
	{
		grn_table_get_key(ctx, so->searched, recordID,
						  &recordID, sizeof(grn_id));
	}

	return recordID;
}

#ifdef PGRN_SUPPORT_INDEX_ONLY_SCAN
static void
PGrnGetTupleFillIndexTuple(PGrnScanOpaque so,
						   IndexScanDesc scan)
{
	TupleDesc desc;
	Datum *values;
	bool *isNulls;
	grn_id recordID;
	unsigned int i;

	desc = RelationGetDescr(so->index);
	scan->xs_itupdesc = desc;

	values = palloc(sizeof(Datum) * desc->natts);
	isNulls = palloc(sizeof(bool) * desc->natts);

	recordID = PGrnScanOpaqueResolveID(so);

	for (i = 0; i < desc->natts; i++)
	{
		Form_pg_attribute attribute = desc->attrs[i];
		NameData *name;
		grn_obj *dataColumn;

		name = &(attribute->attname);
		dataColumn = PGrnLookupColumn(so->sourcesTable, name->data, ERROR);
		GRN_BULK_REWIND(&(buffers->general));
		grn_obj_get_value(ctx, dataColumn, recordID, &(buffers->general));
		values[i] = PGrnConvertToDatum(&(buffers->general), attribute->atttypid);
		isNulls[i] = false;
		grn_obj_unlink(ctx, dataColumn);
	}

	scan->xs_itup = index_form_tuple(scan->xs_itupdesc,
									 values,
									 isNulls);

	pfree(values);
	pfree(isNulls);
}
#endif

static bool
pgroonga_gettuple_raw(IndexScanDesc scan,
					  ScanDirection direction)
{
	PGrnScanOpaque so = (PGrnScanOpaque) scan->opaque;

	PGrnEnsureCursorOpened(scan, direction, true);

	if (scan->kill_prior_tuple && so->currentID != GRN_ID_NIL)
	{
		grn_id recordID;

		recordID = PGrnScanOpaqueResolveID(so);
		grn_table_delete_by_id(ctx, so->sourcesTable, recordID);
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
		return false;
	}
	else
	{
		GRN_BULK_REWIND(&(buffers->ctid));
		grn_obj_get_value(ctx, so->ctidAccessor, so->currentID, &(buffers->ctid));
		scan->xs_ctup.t_self = UInt64ToCtid(GRN_UINT64_VALUE(&(buffers->ctid)));

#ifdef PGRN_SUPPORT_INDEX_ONLY_SCAN
		if (scan->xs_want_itup)
			PGrnGetTupleFillIndexTuple(so, scan);
#endif

		return true;
	}
}

/**
 * pgroonga.gettuple() -- amgettuple
 */
Datum
pgroonga_gettuple(PG_FUNCTION_ARGS)
{
	IndexScanDesc scan = (IndexScanDesc) PG_GETARG_POINTER(0);
	ScanDirection direction = (ScanDirection) PG_GETARG_INT32(1);
	bool found;

	found = pgroonga_gettuple_raw(scan, direction);

	PG_RETURN_BOOL(found);
}

static int64
pgroonga_getbitmap_raw(IndexScanDesc scan,
					   TIDBitmap *tbm)
{
	PGrnScanOpaque so = (PGrnScanOpaque) scan->opaque;
	int64 nRecords = 0;

	PGrnEnsureCursorOpened(scan, ForwardScanDirection, false);

	if (so->indexCursor)
	{
		grn_posting *posting;
		grn_id termID;
		while ((posting = grn_index_cursor_next(ctx, so->indexCursor, &termID)))
		{
			ItemPointerData ctid;
			GRN_BULK_REWIND(&(buffers->ctid));
			grn_obj_get_value(ctx, so->ctidAccessor, posting->rid, &(buffers->ctid));
			ctid = UInt64ToCtid(GRN_UINT64_VALUE(&(buffers->ctid)));
			tbm_add_tuples(tbm, &ctid, 1, scan->xs_recheck);
			nRecords++;
		}
	}
	else
	{
		grn_id id;
		while ((id = grn_table_cursor_next(ctx, so->tableCursor)) != GRN_ID_NIL)
		{
			ItemPointerData ctid;
			GRN_BULK_REWIND(&(buffers->ctid));
			grn_obj_get_value(ctx, so->ctidAccessor, id, &(buffers->ctid));
			ctid = UInt64ToCtid(GRN_UINT64_VALUE(&(buffers->ctid)));
			tbm_add_tuples(tbm, &ctid, 1, scan->xs_recheck);
			nRecords++;
		}
	}

	return nRecords;
}

#ifdef PGRN_SUPPORT_BITMAP_INDEX
/**
 * pgroonga.getbitmap() -- amgetbitmap
 */
Datum
pgroonga_getbitmap(PG_FUNCTION_ARGS)
{
	IndexScanDesc scan = (IndexScanDesc) PG_GETARG_POINTER(0);
	TIDBitmap *tbm = (TIDBitmap *) PG_GETARG_POINTER(1);
	int64 nRecords;

	nRecords = pgroonga_getbitmap_raw(scan, tbm);

	PG_RETURN_INT64(nRecords);
}
#endif

static void
pgroonga_rescan_raw(IndexScanDesc scan,
					ScanKey keys,
					int nKeys,
					ScanKey orderBys,
					int nOrderBys)
{
	PGrnScanOpaque so = (PGrnScanOpaque) scan->opaque;

	PGrnScanOpaqueReinit(so);

	if (keys && scan->numberOfKeys > 0)
		memmove(scan->keyData, keys, scan->numberOfKeys * sizeof(ScanKeyData));
}

/**
 * pgroonga.rescan() -- amrescan
 */
Datum
pgroonga_rescan(PG_FUNCTION_ARGS)
{
	IndexScanDesc scan = (IndexScanDesc) PG_GETARG_POINTER(0);
	ScanKey	keys = (ScanKey) PG_GETARG_POINTER(1);
	int nKeys = PG_GETARG_INT32(2);
	ScanKey	orderBys = (ScanKey) PG_GETARG_POINTER(3);
	int nOrderBys = PG_GETARG_INT32(4);

	pgroonga_rescan_raw(scan,
						keys, nKeys,
						orderBys, nOrderBys);

	PG_RETURN_VOID();
}

static void
pgroonga_endscan_raw(IndexScanDesc scan)
{
	PGrnScanOpaque so = (PGrnScanOpaque) scan->opaque;

	PGrnScanOpaqueFin(so);
	pfree(so);
}

/**
 * pgroonga.endscan() -- amendscan
 */
Datum
pgroonga_endscan(PG_FUNCTION_ARGS)
{
	IndexScanDesc scan = (IndexScanDesc) PG_GETARG_POINTER(0);

	pgroonga_endscan_raw(scan);

	PG_RETURN_VOID();
}

static void
PGrnBuildCallbackRaw(Relation index,
					 ItemPointer ctid,
					 Datum *values,
					 bool *isnull,
					 bool tupleIsAlive,
					 void *state)
{
	PGrnBuildState bs = (PGrnBuildState) state;
	uint32_t recordSize;

	if (!tupleIsAlive)
		return;

	recordSize = PGrnInsert(index,
							bs->sourcesTable,
							bs->sourcesCtidColumn,
							values,
							isnull,
							ctid);
	if (bs->needMaxRecordSizeUpdate &&
		recordSize > bs->maxRecordSize)
	{
		bs->maxRecordSize = recordSize;
	}
	bs->nIndexedTuples++;
}

#ifdef PGRN_IS_GREENPLUM
static void
PGrnBuildCallback(Relation index,
				  ItemPointer ctid,
				  Datum *values,
				  bool *isnull,
				  bool tupleIsAlive,
				  void *state)
{
	PGrnBuildCallbackRaw(index,
						 ctid,
						 values,
						 isnull,
						 tupleIsAlive,
						 state);
}
#else
static void
PGrnBuildCallback(Relation index,
				  HeapTuple htup,
				  Datum *values,
				  bool *isnull,
				  bool tupleIsAlive,
				  void *state)
{
	PGrnBuildCallbackRaw(index,
						 &(htup->t_self),
						 values,
						 isnull,
						 tupleIsAlive,
						 state);
}
#endif

static IndexBuildResult *
pgroonga_build_raw(Relation heap,
				   Relation index,
				   IndexInfo *indexInfo)
{
	IndexBuildResult *result;
	double nHeapTuples = 0.0;
	PGrnBuildStateData bs;
	grn_obj supplementaryTables;
	grn_obj lexicons;

	if (indexInfo->ii_Unique)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("pgroonga: unique index isn't supported")));

	bs.sourcesTable = NULL;
	bs.nIndexedTuples = 0.0;
	bs.needMaxRecordSizeUpdate = PGrnNeedMaxRecordSizeUpdate(index);
	bs.maxRecordSize = 0;

	GRN_PTR_INIT(&supplementaryTables, GRN_OBJ_VECTOR, GRN_ID_NIL);
	GRN_PTR_INIT(&lexicons, GRN_OBJ_VECTOR, GRN_ID_NIL);
	PG_TRY();
	{
		PGrnCreate(index,
				   &(bs.sourcesTable),
				   &(bs.sourcesCtidColumn),
				   &supplementaryTables,
				   &lexicons);
		nHeapTuples = IndexBuildHeapScan(heap, index, indexInfo, true,
										 PGrnBuildCallback, &bs);
		PGrnSetSources(index, bs.sourcesTable);
	}
	PG_CATCH();
	{
		size_t i, n;

		n = GRN_BULK_VSIZE(&lexicons) / sizeof(grn_obj *);
		for (i = 0; i < n; i++)
		{
			grn_obj *lexicon;
			lexicon = GRN_PTR_VALUE_AT(&lexicons, i);
			grn_obj_remove(ctx, lexicon);
		}
		GRN_OBJ_FIN(ctx, &lexicons);

		n = GRN_BULK_VSIZE(&supplementaryTables) / sizeof(grn_obj *);
		for (i = 0; i < n; i++)
		{
			grn_obj *supplementaryTable;
			supplementaryTable = GRN_PTR_VALUE_AT(&supplementaryTables, i);
			grn_obj_remove(ctx, supplementaryTable);
		}
		GRN_OBJ_FIN(ctx, &supplementaryTables);

		if (bs.sourcesTable)
			grn_obj_remove(ctx, bs.sourcesTable);

		PG_RE_THROW();
	}
	PG_END_TRY();
	GRN_OBJ_FIN(ctx, &lexicons);
	GRN_OBJ_FIN(ctx, &supplementaryTables);

	result = (IndexBuildResult *) palloc(sizeof(IndexBuildResult));
	result->heap_tuples = nHeapTuples;
	result->index_tuples = bs.nIndexedTuples;

	if (bs.needMaxRecordSizeUpdate)
	{
		PGrnUpdateMaxRecordSize(index, bs.maxRecordSize);
	}

	return result;
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

	result = pgroonga_build_raw(heap, index, indexInfo);

	PG_RETURN_POINTER(result);
}

static void
pgroonga_buildempty_raw(Relation index)
{
	grn_obj *sourcesTable = NULL;
	grn_obj *sourcesCtidColumn = NULL;
	grn_obj supplementaryTables;
	grn_obj lexicons;

	GRN_PTR_INIT(&supplementaryTables, GRN_OBJ_VECTOR, GRN_ID_NIL);
	GRN_PTR_INIT(&lexicons, GRN_OBJ_VECTOR, GRN_ID_NIL);
	PG_TRY();
	{
		PGrnCreate(index,
				   &sourcesTable,
				   &sourcesCtidColumn,
				   &supplementaryTables,
				   &lexicons);
		PGrnSetSources(index, sourcesTable);
	}
	PG_CATCH();
	{
		size_t i, n;

		n = GRN_BULK_VSIZE(&lexicons) / sizeof(grn_obj *);
		for (i = 0; i < n; i++)
		{
			grn_obj *lexicon;
			lexicon = GRN_PTR_VALUE_AT(&lexicons, i);
			grn_obj_remove(ctx, lexicon);
		}
		GRN_OBJ_FIN(ctx, &lexicons);

		n = GRN_BULK_VSIZE(&supplementaryTables) / sizeof(grn_obj *);
		for (i = 0; i < n; i++)
		{
			grn_obj *supplementaryTable;
			supplementaryTable = GRN_PTR_VALUE_AT(&supplementaryTables, i);
			grn_obj_remove(ctx, supplementaryTable);
		}
		GRN_OBJ_FIN(ctx, &supplementaryTables);

		if (sourcesTable)
			grn_obj_remove(ctx, sourcesTable);

		PG_RE_THROW();
	}
	PG_END_TRY();
	GRN_OBJ_FIN(ctx, &lexicons);
	GRN_OBJ_FIN(ctx, &supplementaryTables);
}

/**
 * pgroonga.buildempty() -- ambuildempty
 */
Datum
pgroonga_buildempty(PG_FUNCTION_ARGS)
{
	Relation index = (Relation) PG_GETARG_POINTER(0);

	pgroonga_buildempty_raw(index);

	PG_RETURN_VOID();
}

static IndexBulkDeleteResult *
PGrnBulkDeleteResult(IndexVacuumInfo *info, grn_obj *sourcesTable)
{
	IndexBulkDeleteResult *stats;

	stats = (IndexBulkDeleteResult *) palloc0(sizeof(IndexBulkDeleteResult));
	stats->num_pages = (BlockNumber) 1;	/* TODO: sizeof index / BLCKSZ */

	/* table might be NULL if index is corrupted */
	if (sourcesTable)
		stats->num_index_tuples = grn_table_size(ctx, sourcesTable);
	else
		stats->num_index_tuples = 0;

	return stats;
}

static IndexBulkDeleteResult *
pgroonga_bulkdelete_raw(IndexVacuumInfo *info,
						IndexBulkDeleteResult *stats,
						IndexBulkDeleteCallback callback,
						void *callbackState)
{
	Relation index = info->index;
	grn_obj	*sourcesTable;
	grn_table_cursor *cursor;
	double nRemovedTuples;

	sourcesTable = PGrnLookupSourcesTable(index, WARNING);

	if (!stats)
		stats = PGrnBulkDeleteResult(info, sourcesTable);

	if (!sourcesTable || !callback)
		return stats;

	nRemovedTuples = 0;

	cursor = grn_table_cursor_open(ctx, sourcesTable,
								   NULL, 0, NULL, 0,
								   0, -1, 0);
	if (!cursor)
		ereport(ERROR,
				(errcode(ERRCODE_SYSTEM_ERROR),
				 errmsg("pgroonga: failed to open cursor: %s", ctx->errbuf)));

	PG_TRY();
	{
		grn_id id;
		grn_obj *sourcesCtidColumn;
		PGrnJSONBBulkDeleteData jsonbData;

		sourcesCtidColumn = PGrnLookupSourcesCtidColumn(index, ERROR);

		jsonbData.index = index;
		jsonbData.sourcesTable = sourcesTable;
		PGrnJSONBBulkDeleteInit(&jsonbData);

		while ((id = grn_table_cursor_next(ctx, cursor)) != GRN_ID_NIL)
		{
			ItemPointerData	ctid;

			CHECK_FOR_INTERRUPTS();

			GRN_BULK_REWIND(&(buffers->ctid));
			grn_obj_get_value(ctx, sourcesCtidColumn, id, &(buffers->ctid));
			ctid = UInt64ToCtid(GRN_UINT64_VALUE(&(buffers->ctid)));
			if (callback(&ctid, callbackState))
			{
				jsonbData.id = id;
				PGrnJSONBBulkDeleteRecord(&jsonbData);

				grn_table_cursor_delete(ctx, cursor);

				nRemovedTuples += 1;
			}
		}

		PGrnJSONBBulkDeleteFin(&jsonbData);

		grn_table_cursor_close(ctx, cursor);
	}
	PG_CATCH();
	{
		grn_table_cursor_close(ctx, cursor);
		PG_RE_THROW();
	}
	PG_END_TRY();

	stats->tuples_removed = nRemovedTuples;

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
	void *callbackState = (void *) PG_GETARG_POINTER(3);

	stats = pgroonga_bulkdelete_raw(info,
									stats,
									callback,
									callbackState);

	PG_RETURN_POINTER(stats);
}

#ifdef PGRN_SUPPORT_FILE_NODE_ID_TO_RELATION_ID
static bool
PGrnIsValidRelationFileNodeID(Oid relationFileNodeID)
{
	bool valid = false;
	Relation tableSpaces;
	HeapScanDesc scan;

	tableSpaces = heap_open(TableSpaceRelationId, AccessShareLock);
	scan = heap_beginscan_catalog(tableSpaces, 0, NULL);
	while (!valid)
	{
		HeapTuple tuple;
		Oid relationID;
		Relation relation;

		tuple = heap_getnext(scan, ForwardScanDirection);

		if (!HeapTupleIsValid(tuple))
			break;

		relationID = RelidByRelfilenode(HeapTupleGetOid(tuple),
										relationFileNodeID);
		if (!OidIsValid(relationID))
			continue;

		LockRelationOid(relationID, AccessShareLock);
		relation = RelationIdGetRelation(relationID);
		if (RelationIsValid(relation))
		{
			RelationClose(relation);
			valid = true;
		}
		UnlockRelationOid(relationID, AccessShareLock);
	}
	heap_endscan(scan);
	heap_close(tableSpaces, AccessShareLock);

	return valid;
}
#endif

static void
PGrnRemoveUnusedTables(void)
{
#ifdef PGRN_SUPPORT_FILE_NODE_ID_TO_RELATION_ID
	grn_table_cursor *cursor;
	const char *min = PGrnSourcesTableNamePrefix;

	cursor = grn_table_cursor_open(ctx, grn_ctx_db(ctx),
								   min, strlen(min),
								   NULL, 0,
								   0, -1, GRN_CURSOR_BY_KEY|GRN_CURSOR_PREFIX);
	while (grn_table_cursor_next(ctx, cursor) != GRN_ID_NIL)
	{
		char *name;
		char *nameEnd;
		int nameSize;
		Oid relationFileNodeID;
		unsigned int i;

		nameSize = grn_table_cursor_get_key(ctx, cursor, (void **)&name);
		nameEnd = name + nameSize;
		relationFileNodeID = strtol(name + strlen(min), &nameEnd, 10);
		if (nameEnd[0] == '.')
			continue;

		if (PGrnIsValidRelationFileNodeID(relationFileNodeID))
			continue;

		for (i = 0; true; i++)
		{
			char tableName[GRN_TABLE_MAX_KEY_SIZE];
			grn_obj *lexicon;

			snprintf(tableName, sizeof(tableName),
					 PGrnLexiconNameFormat, relationFileNodeID, i);
			lexicon = grn_ctx_get(ctx, tableName, -1);
			if (!lexicon)
				break;

			PGrnRemoveColumns(lexicon);
		}

		{
			char tableName[GRN_TABLE_MAX_KEY_SIZE];
			snprintf(tableName, sizeof(tableName),
					 PGrnSourcesTableNameFormat, relationFileNodeID);
			PGrnRemoveObject(tableName);
		}

		for (i = 0; true; i++)
		{
			char tableName[GRN_TABLE_MAX_KEY_SIZE];
			grn_obj *lexicon;

			snprintf(tableName, sizeof(tableName),
					 PGrnLexiconNameFormat, relationFileNodeID, i);
			lexicon = grn_ctx_get(ctx, tableName, -1);
			if (!lexicon)
				break;

			PGrnRemoveObject(tableName);
		}

		PGrnJSONBRemoveUnusedTables(relationFileNodeID);
	}
	grn_table_cursor_close(ctx, cursor);
#endif
}

static IndexBulkDeleteResult *
pgroonga_vacuumcleanup_raw(IndexVacuumInfo *info,
						   IndexBulkDeleteResult *stats)
{
	if (!stats)
	{
		grn_obj *sourcesTable;
		sourcesTable = PGrnLookupSourcesTable(info->index, WARNING);
		stats = PGrnBulkDeleteResult(info, sourcesTable);
	}

	PGrnRemoveUnusedTables();

	return stats;
}

/**
 * pgroonga.vacuumcleanup() -- amvacuumcleanup
 */
Datum
pgroonga_vacuumcleanup(PG_FUNCTION_ARGS)
{
	IndexVacuumInfo *info = (IndexVacuumInfo *) PG_GETARG_POINTER(0);
	IndexBulkDeleteResult *stats = (IndexBulkDeleteResult *) PG_GETARG_POINTER(1);

	stats = pgroonga_vacuumcleanup_raw(info, stats);

	PG_RETURN_POINTER(stats);
}

static bool
pgroonga_canreturn_raw(Relation index,
					   int nthAttribute)
{
	TupleDesc desc;
	unsigned int i;

	if (PGrnIndexStatusGetMaxRecordSize(index) >= INDEX_SIZE_MASK)
		return false;

	desc = RelationGetDescr(index);
	for (i = 0; i < desc->natts; i++)
	{
		Form_pg_attribute attribute = desc->attrs[i];
		if (PGrnAttributeIsJSONB(attribute->atttypid))
		{
			return false;
		}

		if (PGrnIsForPrefixSearchIndex(index, i))
		{
			return false;
		}
	}

	return true;
}

/**
 * pgroonga.canreturn() -- amcanreturn
 */
Datum
pgroonga_canreturn(PG_FUNCTION_ARGS)
{
	Relation index = (Relation) PG_GETARG_POINTER(0);
	int nthAttribute = PG_GETARG_INT32(1);
	bool can;

	can = pgroonga_canreturn_raw(index, nthAttribute);

	PG_RETURN_BOOL(can);
}

static void
PGrnCostEstimateUpdateSelectivity(IndexPath *path)
{
	IndexOptInfo *indexInfo = path->indexinfo;
	Relation index;
	grn_obj *sourcesTable;
	ListCell *cell;

	index = RelationIdGetRelation(indexInfo->indexoid);
	PGrnWALApply(index);
	sourcesTable = PGrnLookupSourcesTable(index, ERROR);

	foreach(cell, path->indexquals)
	{
		Node *clause = (Node *) lfirst(cell);
		RestrictInfo *info;
		OpExpr *expr;
		int strategy;
		Oid leftType;
		Oid rightType;
		Node *leftNode;
		Node *rightNode;
		Var *var;
		int nthAttribute = InvalidAttrNumber;
		Oid opFamily = InvalidOid;
		ScanKeyData key;
		PGrnSearchData data;

		if (!IsA(clause, RestrictInfo))
			continue;

		info = (RestrictInfo *) clause;

		if (!IsA(info->clause, OpExpr))
			continue;

		expr = (OpExpr *) info->clause;

		leftNode = get_leftop(info->clause);
		rightNode = get_rightop(info->clause);

		if (!IsA(leftNode, Var))
			continue;
		if (!IsA(rightNode, Const))
			continue;

		var = (Var *) leftNode;
		{
			int i;

			for (i = 0; i < indexInfo->ncolumns; i++)
			{
				if (indexInfo->indexkeys[i] == var->varattno)
				{
					nthAttribute = i + 1;
					break;
				}
			}
		}
		if (!AttributeNumberIsValid(nthAttribute))
			continue;

		opFamily = index->rd_opfamily[nthAttribute - 1];
		get_op_opfamily_properties(expr->opno,
								   opFamily,
								   false,
								   &strategy,
								   &leftType,
								   &rightType);

		key.sk_flags = 0;
		key.sk_attno = nthAttribute;
		key.sk_strategy = strategy;
		key.sk_argument = ((Const *) rightNode)->constvalue;
		PGrnSearchDataInit(&data, sourcesTable);
		if (PGrnSearchBuildCondition(index, &key, &data))
		{
			unsigned int estimatedSize;
			unsigned int nRecords;

			if (data.isEmptyCondition)
			{
				estimatedSize = 0;
			}
			else
			{
				estimatedSize = grn_expr_estimate_size(ctx, data.expression);
			}

			nRecords = grn_table_size(ctx, sourcesTable);
			if (estimatedSize > nRecords)
				estimatedSize = nRecords;
			if (estimatedSize == nRecords)
			{
				/* TODO: estimatedSize == nRecords means
				 * estimation isn't support in Groonga. We should
				 * support it in Groonga. */
				info->norm_selec = 0.01;
			}
			else
			{
				info->norm_selec = (double) estimatedSize / (double) nRecords;
			}
		}
		else
		{
			info->norm_selec = 0.0;
		}
		PGrnSearchDataFree(&data);
	}

	RelationClose(index);
}

static void
pgroonga_costestimate_raw(PlannerInfo *root,
						  IndexPath *path,
						  double loopCount,
						  Cost *indexStartupCost,
						  Cost *indexTotalCost,
						  Selectivity *indexSelectivity,
						  double *indexCorrelation)
{
	PGrnCostEstimateUpdateSelectivity(path);
	*indexSelectivity = clauselist_selectivity(root,
											   path->indexquals,
											   path->indexinfo->rel->relid,
											   JOIN_INNER,
											   NULL);

	*indexStartupCost = 0.0; /* TODO */
	*indexTotalCost = 0.0; /* TODO */
	*indexCorrelation = 0.0;
}

/**
 * pgroonga.costestimate() -- amcostestimate
 */
Datum
pgroonga_costestimate(PG_FUNCTION_ARGS)
{
	PlannerInfo *root = (PlannerInfo *) PG_GETARG_POINTER(0);
	IndexPath *path = (IndexPath *) PG_GETARG_POINTER(1);
	double loopCount = PG_GETARG_FLOAT8(2);
	Cost *indexStartupCost = (Cost *) PG_GETARG_POINTER(3);
	Cost *indexTotalCost = (Cost *) PG_GETARG_POINTER(4);
	Selectivity *indexSelectivity = (Selectivity *) PG_GETARG_POINTER(5);
	double *indexCorrelation = (double *) PG_GETARG_POINTER(6);

	pgroonga_costestimate_raw(root,
							  path,
							  loopCount,
							  indexStartupCost,
							  indexTotalCost,
							  indexSelectivity,
							  indexCorrelation);

	PG_RETURN_VOID();
}

#ifdef PGRN_SUPPORT_CREATE_ACCESS_METHOD
static bool
pgroonga_validate_raw(Oid opClassOid)
{
	return true;
}

Datum
pgroonga_handler(PG_FUNCTION_ARGS)
{
	IndexAmRoutine *routine = makeNode(IndexAmRoutine);

	routine->amstrategies = PGRN_N_STRATEGIES;
	routine->amsupport    = 0;
	routine->amcanorder = true;
	routine->amcanorderbyop = true;
	routine->amcanbackward = true;
	routine->amcanunique = true;
	routine->amcanmulticol = true;
	routine->amoptionalkey = true;
	routine->amsearcharray = true;
	routine->amsearchnulls = false;
	routine->amstorage = false;
	routine->amclusterable = true;
	routine->ampredlocks = false;
	routine->amkeytype = 0;

	routine->aminsert = pgroonga_insert_raw;
	routine->ambeginscan = pgroonga_beginscan_raw;
	routine->amgettuple = pgroonga_gettuple_raw;
	routine->amgetbitmap = pgroonga_getbitmap_raw;
	routine->amrescan = pgroonga_rescan_raw;
	routine->amendscan = pgroonga_endscan_raw;
	routine->ammarkpos = NULL;
	routine->amrestrpos = NULL;
	routine->ambuild = pgroonga_build_raw;
	routine->ambuildempty = pgroonga_buildempty_raw;
	routine->ambulkdelete = pgroonga_bulkdelete_raw;
	routine->amvacuumcleanup = pgroonga_vacuumcleanup_raw;
	routine->amcanreturn = pgroonga_canreturn_raw;
	routine->amcostestimate = pgroonga_costestimate_raw;
	routine->amoptions = pgroonga_options_raw;
	routine->amvalidate = pgroonga_validate_raw;

	PG_RETURN_POINTER(routine);
}
#endif
