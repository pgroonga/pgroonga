/*
 * IDENTIFICATION
 *	pgroonga.c
 */

#include "pgroonga.h"

#include <access/relscan.h>
#include <catalog/index.h>
#include <catalog/pg_tablespace.h>
#include <mb/pg_wchar.h>
#include <miscadmin.h>
#include <storage/ipc.h>
#include <storage/lmgr.h>
#include <utils/selfuncs.h>

#include <groonga.h>

PG_MODULE_MAGIC;

typedef struct GrnBuildStateData
{
	grn_obj	*idsTable;
	grn_ii_buffer *buffer;
	double nIndexedTuples;
} GrnBuildStateData;

typedef GrnBuildStateData *GrnBuildState;

typedef struct GrnScanOpaqueData
{
	grn_obj *idsTable;
	grn_obj *searched;
	grn_obj *sorted;
	grn_obj *targetTable;
	grn_table_cursor *cursor;
	grn_obj *keyAccessor;
	grn_id currentID;
} GrnScanOpaqueData;

typedef GrnScanOpaqueData *GrnScanOpaque;

PG_FUNCTION_INFO_V1(pgroonga_contains_text);
PG_FUNCTION_INFO_V1(pgroonga_contains_bpchar);

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
static grn_obj inspectBuffer;

#ifdef PGROONGA_DEBUG
static const char *
GrnInspect(grn_obj *object)
{
	GRN_BULK_REWIND(&inspectBuffer);
	grn_inspect(ctx, &inspectBuffer, object);
	GRN_TEXT_PUTC(ctx, &inspectBuffer, '\0');
	return GRN_TEXT_VALUE(&inspectBuffer);
}
#endif

static grn_encoding
GrnGetEncoding(void)
{
	int	enc = GetDatabaseEncoding();

	if (pg_encoding_max_length(enc) > 1)
		return GRN_ENC_NONE;

	switch (enc)
	{
	case PG_EUC_JP:
	case PG_EUC_JIS_2004:
		return GRN_ENC_EUC_JP;
	case PG_UTF8:
		return GRN_ENC_UTF8;
	case PG_LATIN1:
		return GRN_ENC_LATIN1;
	case PG_KOI8R:
		return GRN_ENC_KOI8R;
	default:
		elog(WARNING,
			"groonga: use default encoding instead of '%s'",
			GetDatabaseEncodingName());
		return GRN_ENC_DEFAULT;
	}
}

static void
GrnEnsureDatabase(void)
{
	char path[MAXPGPATH];
	grn_obj	*db;

	GRN_CTX_SET_ENCODING(ctx, GrnGetEncoding());
	join_path_components(path,
						 GetDatabasePath(MyDatabaseId, DEFAULTTABLESPACE_OID),
						 GrnDatabaseBasename);

	db = grn_db_open(ctx, path);
	if (db)
		return;

	db = grn_db_create(ctx, path, NULL);
	if (!db)
		ereport(ERROR,
				(errcode(ERRCODE_IO_ERROR),
				 errmsg("groonga: failed to create database: <%s>: %s",
						path, ctx->errbuf)));
}

static void
GrnOnProcExit(int code, Datum arg)
{
	grn_obj *db;

	GRN_OBJ_FIN(ctx, &inspectBuffer);
	GRN_OBJ_FIN(ctx, &buffer);

	db = grn_ctx_db(ctx);
	if (db)
		grn_obj_close(ctx, db);

	grn_ctx_fin(ctx);
	grn_fin();
}

void
_PG_init(void)
{
	if (grn_init() != GRN_SUCCESS)
		ereport(ERROR,
				(errcode(ERRCODE_SYSTEM_ERROR),
				 errmsg("groonga: failed to initialize Groonga")));
	if (grn_ctx_init(ctx, 0))
		ereport(ERROR,
				(errcode(ERRCODE_SYSTEM_ERROR),
				 errmsg("groonga: failed to initialize Groonga context")));

	on_proc_exit(GrnOnProcExit, 0);

	GRN_VOID_INIT(&buffer);
	GRN_TEXT_INIT(&inspectBuffer, 0);

	GrnEnsureDatabase();
}

static int
GrnRCToPgErrorCode(grn_rc rc)
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
GrnCheck(const char *message)
{
	if (ctx->rc == GRN_SUCCESS)
		return GRN_TRUE;

	ereport(ERROR,
			(errcode(GrnRCToPgErrorCode(ctx->rc)),
			 errmsg("groonga: %s: %s", message, ctx->errbuf)));
	return GRN_FALSE;
}

/*
 * Support functions and type-specific routines
 */

static grn_builtin_type
GrnGetType(Relation index, AttrNumber n)
{
	FmgrInfo *function;
	TupleDesc desc = RelationGetDescr(index);
	Datum type;

	function = index_getprocinfo(index, n + 1, GrnTypeOfProc);
	type = FunctionCall2(function,
						 ObjectIdGetDatum(desc->attrs[n]->atttypid),
						 Int32GetDatum(desc->attrs[n]->atttypmod));
	return (grn_builtin_type) DatumGetInt32(type);
}

static void
GrnSetValue(Relation index, AttrNumber n, grn_obj *buffer, Datum value)
{
	FmgrInfo *function;

	function = index_getprocinfo(index, n + 1, GrnSetValueProc);
	FunctionCall3(function,
				  PointerGetDatum(ctx), PointerGetDatum(buffer),
				  value);
}

static void
GrnGetValue(Relation index, AttrNumber n, Datum value, grn_obj *buffer)
{
	FmgrInfo *function;

	function = index_getprocinfo(index, n + 1, GrnGetValueProc);
	FunctionCall3(function,
				  value,
				  PointerGetDatum(ctx), PointerGetDatum(buffer));
}

static grn_obj *
GrnLookup(const char *name, int errorLevel)
{
	grn_obj *object = grn_ctx_get(ctx, name, strlen(name));
	if (!object)
		ereport(errorLevel,
				(errcode(ERRCODE_INVALID_NAME),
				 errmsg("groonga: object isn't found: <%s>", name)));
	return object;
}

static grn_obj *
GrnLookupIDsTable(Relation index, int errorLevel)
{
	char name[GRN_TABLE_MAX_KEY_SIZE];

	snprintf(name, sizeof(name), GrnIDsTableNameFormat, index->rd_node.relNode);
	return GrnLookup(name, errorLevel);
}

static grn_obj *
GrnLookupIndexColumn(Relation index, int errorLevel)
{
	char name[GRN_TABLE_MAX_KEY_SIZE];

	snprintf(name, sizeof(name),
			 GrnLexiconNameFormat ".%s",
			 index->rd_node.relNode, GrnIndexColumnName);
	return GrnLookup(name, errorLevel);
}

static grn_obj *
GrnCreateTable(const char *name,
			   grn_obj_flags flags,
			   grn_obj *type)
{
	grn_obj	*table;

	table = grn_table_create(ctx,
							 name, strlen(name), NULL,
							 GRN_OBJ_PERSISTENT | flags,
							 type,
							 NULL);
	GrnCheck("groonga: failed to create table");

	return table;
}

static grn_obj *
GrnCreateColumn(grn_obj	*table,
				const char *name,
				grn_obj_flags flags,
				grn_obj	*type)
{
	grn_obj *column;

    column = grn_column_create(ctx, table,
							   name, strlen(name), NULL,
							   GRN_OBJ_PERSISTENT | flags,
							   type);
	GrnCheck("groonga: failed to create column");

	return column;
}

/**
 * GrnCreate
 *
 * @param	ctx
 * @param	index
 */
static void
GrnCreate(Relation index, grn_obj **idsTable,
		  grn_obj **lexicon, grn_obj **indexColumn)
{
	char idsTableName[GRN_TABLE_MAX_KEY_SIZE];
	char lexiconName[GRN_TABLE_MAX_KEY_SIZE];
	grn_id typeID = GRN_ID_NIL;
	int i;
	TupleDesc desc;
	Oid relNode = index->rd_node.relNode;

	desc = RelationGetDescr(index);

	snprintf(idsTableName, sizeof(idsTableName),
			 GrnIDsTableNameFormat, relNode);
	*idsTable = GrnCreateTable(idsTableName,
							   GRN_OBJ_TABLE_PAT_KEY,
							   grn_ctx_at(ctx, GRN_DB_UINT64));

	for (i = 0; i < desc->natts; i++)
	{
		grn_id attributeTypeID;

		attributeTypeID = GrnGetType(index, i);
		if (typeID == GRN_ID_NIL)
			typeID = attributeTypeID;

		if (attributeTypeID != typeID)
		{
			/* TODO: Show details */
			ereport(ERROR,
					(errcode(ERRCODE_INTERNAL_ERROR),
					 errmsg("groonga: must be the same type columns "
							"for multiple column index")));
		}
	}

	switch (typeID)
	{
	case GRN_DB_TEXT:
	case GRN_DB_LONG_TEXT:
		typeID = GRN_DB_SHORT_TEXT;
		break;
	}

	snprintf(lexiconName, sizeof(lexiconName), GrnLexiconNameFormat, relNode);
	*lexicon = GrnCreateTable(lexiconName,
							  GRN_OBJ_TABLE_PAT_KEY,
							  grn_ctx_at(ctx, typeID));
	if (typeID == GRN_DB_SHORT_TEXT)
	{
		grn_obj_set_info(ctx, *lexicon, GRN_INFO_NORMALIZER,
						 GrnLookup("NormalizerAuto", WARNING));
		grn_obj_set_info(ctx, *lexicon, GRN_INFO_DEFAULT_TOKENIZER,
						 grn_ctx_at(ctx, GRN_DB_BIGRAM));
	}

	{
		grn_obj_flags flags = GRN_OBJ_COLUMN_INDEX;
		if (typeID == GRN_DB_SHORT_TEXT)
			flags |= GRN_OBJ_WITH_POSITION;
		if (desc->natts > 1)
			flags |= GRN_OBJ_WITH_SECTION;
		*indexColumn = GrnCreateColumn(*lexicon,
									   GrnIndexColumnName,
									   flags,
									   *idsTable);
	}
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

static void
GrnLock(Relation index, LOCKMODE mode)
{
	const RelFileNode *rnode = &index->rd_node;
	LockDatabaseObject(rnode->spcNode,
					   rnode->dbNode,
					   rnode->relNode,
					   mode);
}

static void
GrnUnlock(Relation index, LOCKMODE mode)
{
	const RelFileNode *rnode = &index->rd_node;
	UnlockDatabaseObject(rnode->spcNode,
						 rnode->dbNode,
						 rnode->relNode,
						 mode);
}

static grn_bool
pgroonga_contains_raw(const char *text, unsigned int text_size,
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
 * pgroonga.contains(doc text, key text) : bool
 */
Datum
pgroonga_contains_text(PG_FUNCTION_ARGS)
{
	text *doc = PG_GETARG_TEXT_PP(0);
	text *key = PG_GETARG_TEXT_PP(1);
	grn_bool contained;

	contained = pgroonga_contains_raw(VARDATA_ANY(doc), VARSIZE_ANY_EXHDR(doc),
									  VARDATA_ANY(key), VARSIZE_ANY_EXHDR(key));
	PG_RETURN_BOOL(contained);
}

/**
 * pgroonga.contains(doc bpchar, key bpchar) : bool
 */
Datum
pgroonga_contains_bpchar(PG_FUNCTION_ARGS)
{
	BpChar *doc = PG_GETARG_BPCHAR_PP(0);
	BpChar *key = PG_GETARG_BPCHAR_PP(1);
	grn_bool contained;

	contained =
		pgroonga_contains_raw(VARDATA_ANY(doc), pgroonga_bpchar_size(doc),
							  VARDATA_ANY(key), pgroonga_bpchar_size(key));
	PG_RETURN_BOOL(contained);
}

static void
GrnInsert(grn_ctx *ctx,
		  Relation index,
		  grn_obj *idsTable,
		  grn_obj *indexColumn,
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
		unsigned int sectionID = i + 1;

		if (isnull[i])
			continue;

		index_getprocinfo(index, i, GrnGetValueProc);
		grn_obj_reinit(ctx, &buffer, GrnGetType(index, i), 0);
		GrnSetValue(index, i, &buffer, values[i]);
		grn_column_index_update(ctx, indexColumn, id, sectionID, NULL, &buffer);
		if (!GrnCheck("groonga: failed to update index")) {
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
	grn_obj *idsTable = GrnLookupIDsTable(index, ERROR);
	grn_obj *indexColumn = GrnLookupIndexColumn(index, ERROR);

	GrnLock(index, ExclusiveLock);
	GrnInsert(ctx, index, idsTable, indexColumn, values, isnull, ht_ctid);
	GrnUnlock(index, ExclusiveLock);

	PG_RETURN_BOOL(true);
}

static void
GrnScanOpaqueInit(GrnScanOpaque so, Relation index)
{
	so->idsTable = GrnLookupIDsTable(index, ERROR);
	so->searched = NULL;
	so->sorted = NULL;
	so->targetTable = NULL;
	so->cursor = NULL;
	so->keyAccessor = NULL;
	so->currentID = GRN_ID_NIL;
}

static void
GrnScanOpaqueReinit(GrnScanOpaque so)
{
	so->currentID = GRN_ID_NIL;
	if (so->keyAccessor)
	{
		grn_obj_unlink(ctx, so->keyAccessor);
		so->keyAccessor = NULL;
	}
	if (so->cursor)
	{
		grn_table_cursor_close(ctx, so->cursor);
		so->cursor = NULL;
	}
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
	GrnScanOpaque so;

	scan = RelationGetIndexScan(index, nkeys, norderbys);

	so = (GrnScanOpaque) palloc(sizeof(GrnScanOpaqueData));
	GrnScanOpaqueInit(so, index);

	scan->opaque = so;

	PG_RETURN_POINTER(scan);
}

static void
GrnSearch(IndexScanDesc scan)
{
	Relation index = scan->indexRelation;
	GrnScanOpaque so = (GrnScanOpaque) scan->opaque;
	grn_obj *indexColumn;
	grn_obj *matchColumns, *matchColumnsVariable;
	grn_obj *expression, *expressionVariable;
	int i, nExpressions = 0;

	if (scan->numberOfKeys == 0)
		return;

	GRN_EXPR_CREATE_FOR_QUERY(ctx, so->idsTable,
							  matchColumns, matchColumnsVariable);
	indexColumn = GrnLookupIndexColumn(index, ERROR);
	grn_expr_append_obj(ctx, matchColumns, indexColumn, GRN_OP_PUSH, 1);

	GRN_EXPR_CREATE_FOR_QUERY(ctx, so->idsTable,
							  expression, expressionVariable);

	for (i = 0; i < scan->numberOfKeys; i++)
	{
		ScanKey key = &(scan->keyData[i]);
		grn_bool isValidStrategy = GRN_TRUE;

		/* NULL key is not supported */
		if (key->sk_flags & SK_ISNULL)
			continue;

		grn_obj_reinit(ctx, &buffer, GrnGetType(index, key->sk_attno - 1), 0);
		GrnGetValue(index, key->sk_attno - 1, key->sk_argument, &buffer);

		grn_expr_append_obj(ctx, expression, matchColumns, GRN_OP_PUSH, 1);
		grn_expr_append_obj(ctx, expression, &buffer, GRN_OP_PUSH, 1);

		switch (key->sk_strategy)
		{
		case GrnLessStrategyNumber:
			grn_expr_append_op(ctx, expression, GRN_OP_LESS, 2);
			break;
		case GrnLessEqualStrategyNumber:
			grn_expr_append_op(ctx, expression, GRN_OP_LESS_EQUAL, 2);
			break;
		case GrnEqualStrategyNumber:
			grn_expr_append_op(ctx, expression, GRN_OP_EQUAL, 2);
			break;
		case GrnGreaterEqualStrategyNumber:
			grn_expr_append_op(ctx, expression, GRN_OP_GREATER_EQUAL, 2);
			break;
		case GrnGreaterStrategyNumber:
			grn_expr_append_op(ctx, expression, GRN_OP_GREATER, 2);
			break;
		case GrnNotEqualStrategyNumber:
			grn_expr_append_op(ctx, expression, GRN_OP_NOT_EQUAL, 2);
			break;
		case GrnContainStrategyNumber:
			grn_expr_append_op(ctx, expression, GRN_OP_MATCH, 2);
			break;
		default:
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("unexpected strategy number: %d", key->sk_strategy)));
			isValidStrategy = GRN_FALSE;
			break;
		}

		if (!isValidStrategy)
			continue;

		if (nExpressions > 0)
			grn_expr_append_op(ctx, expression, GRN_OP_AND, 2);
		nExpressions++;
	}

	so->searched = grn_table_create(ctx, NULL, 0, NULL,
									GRN_OBJ_TABLE_HASH_KEY | GRN_OBJ_WITH_SUBREC,
									so->idsTable, 0);
    grn_table_select(ctx, so->idsTable, expression, so->searched, GRN_OP_OR);
	grn_obj_unlink(ctx, expression);
	grn_obj_unlink(ctx, matchColumns);
}

static void
GrnSort(IndexScanDesc scan)
{
	/* TODO */
}

static void
GrnOpenCursor(IndexScanDesc scan, ScanDirection dir)
{
	GrnScanOpaque so = (GrnScanOpaque) scan->opaque;
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

	so->cursor = grn_table_cursor_open(ctx, table,
									   NULL, 0, NULL, 0,
									   offset, limit, flags);
	so->keyAccessor = grn_obj_column(ctx, table,
									 GRN_COLUMN_NAME_KEY,
									 GRN_COLUMN_NAME_KEY_LEN);
}

static void
GrnEnsureCursorOpened(IndexScanDesc scan, ScanDirection dir)
{
	GrnScanOpaque so = (GrnScanOpaque) scan->opaque;

	if (so->cursor)
		return;

	GrnSearch(scan);
	GrnSort(scan);
	GrnOpenCursor(scan, dir);
}


/**
 * pgroonga.gettuple() -- amgettuple
 */
Datum
pgroonga_gettuple(PG_FUNCTION_ARGS)
{
	IndexScanDesc scan = (IndexScanDesc) PG_GETARG_POINTER(0);
	ScanDirection dir = (ScanDirection) PG_GETARG_INT32(1);
	GrnScanOpaque so = (GrnScanOpaque) scan->opaque;

	scan->xs_recheck = false;

	GrnEnsureCursorOpened(scan, dir);

	if (scan->kill_prior_tuple && so->currentID != GRN_ID_NIL)
	{
		grn_obj key;
		GRN_UINT64_INIT(&key, 0);
		GrnLock(scan->indexRelation, ExclusiveLock);
		grn_obj_get_value(ctx, so->keyAccessor, so->currentID, &key);
		grn_table_delete(ctx, so->idsTable,
						 GRN_BULK_HEAD(&key), GRN_BULK_VSIZE(&key));
		GrnUnlock(scan->indexRelation, ExclusiveLock);
		GRN_OBJ_FIN(ctx, &key);
	}

	so->currentID = grn_table_cursor_next(ctx, so->cursor);
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
	GrnScanOpaque so = (GrnScanOpaque) scan->opaque;
	int64 nRecords = 0;
	grn_id id;
	grn_obj key;

	GrnEnsureCursorOpened(scan, ForwardScanDirection);

	GRN_UINT64_INIT(&key, 0);
	while ((id = grn_table_cursor_next(ctx, so->cursor)) != GRN_ID_NIL) {
		ItemPointerData ctid;
		GRN_BULK_REWIND(&key);
		grn_obj_get_value(ctx, so->keyAccessor, id, &key);
		ctid = UInt64ToCtid(GRN_UINT64_VALUE(&key));
		tbm_add_tuples(tbm, &ctid, 1, false);
		nRecords++;
	}
	GRN_OBJ_FIN(ctx, &key);

	PG_RETURN_INT64(nRecords);
}

/**
 * pgroonga.rescan() -- amrescan
 *
 * この段階ではスキャンキーがまだ与えられていない場合がある。
 * まだ検索を行わなず、後から gettuple または getbitmap で検索する。
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
	GrnScanOpaque so = (GrnScanOpaque) scan->opaque;

	GrnScanOpaqueReinit(so);

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
	GrnScanOpaque so = (GrnScanOpaque) scan->opaque;

	GrnScanOpaqueReinit(so);
	pfree(so);

	PG_RETURN_VOID();
}

static void
GrnBuildCallback(Relation index,
				 HeapTuple htup,
				 Datum *values,
				 bool *isnull,
				 bool tupleIsAlive,
				 void *state)
{
	GrnBuildState bs = (GrnBuildState) state;
	TupleDesc desc = RelationGetDescr(index);
	uint64 key = CtidToUInt64(&htup->t_self);
	grn_id id;
	int i;

	id = grn_table_add(ctx, bs->idsTable, &key, sizeof(uint64), NULL);
	for (i = 0; i < desc->natts; i++)
	{
		unsigned int sectionID = i + 1;

		if (isnull[i])
			continue;

		index_getprocinfo(index, i, GrnGetValueProc);
		grn_obj_reinit(ctx, &buffer, GrnGetType(index, i), 0);
		GrnSetValue(index, i, &buffer, values[i]);
		grn_ii_buffer_append(ctx, bs->buffer, id, sectionID, &buffer);
		if (!GrnCheck("groonga: failed to append data to index")) {
			continue;
		}
	}

	bs->nIndexedTuples++;
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
	GrnBuildStateData bs;
	grn_obj *lexicon = NULL;
	grn_obj *indexColumn = NULL;

	if (indexInfo->ii_Unique)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("groonga: unique index isn't supported")));

	bs.idsTable = NULL;
	bs.buffer = NULL;
	bs.nIndexedTuples = 0.0;

	PG_TRY();
	{
		GrnCreate(index, &(bs.idsTable), &lexicon, &indexColumn);
		{
			unsigned long long int updateBufferSize = 10;
			bs.buffer = grn_ii_buffer_open(ctx, (grn_ii *) indexColumn,
											  updateBufferSize);
		}
		nHeapTuples = IndexBuildHeapScan(heap, index, indexInfo, true,
										 GrnBuildCallback, &bs);
		grn_ii_buffer_commit(ctx, bs.buffer);
		grn_ii_buffer_close(ctx, bs.buffer);
	}
	PG_CATCH();
	{
		if (bs.buffer)
			grn_ii_buffer_close(ctx, bs.buffer);
		if (indexColumn)
			grn_obj_remove(ctx, indexColumn);
		if (lexicon)
			grn_obj_remove(ctx, lexicon);
		if (bs.idsTable)
			grn_obj_remove(ctx, bs.idsTable);
		PG_RE_THROW();
	}
	PG_END_TRY();

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
	grn_obj *lexicon = NULL;
	grn_obj *indexColumn = NULL;

	PG_TRY();
	{
		GrnCreate(index, &idsTable, &lexicon, &indexColumn);
	}
	PG_CATCH();
	{
		if (indexColumn)
			grn_obj_remove(ctx, indexColumn);
		if (lexicon)
			grn_obj_remove(ctx, lexicon);
		if (idsTable)
			grn_obj_remove(ctx, idsTable);
		PG_RE_THROW();
	}
	PG_END_TRY();

	PG_RETURN_VOID();
}

static IndexBulkDeleteResult *
GrnBulkDeleteResult(IndexVacuumInfo *info, grn_obj *idsTable)
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

	idsTable = GrnLookupIDsTable(index, WARNING);

	if (!stats)
		stats = GrnBulkDeleteResult(info, idsTable);

	if (!idsTable || !callback)
		PG_RETURN_POINTER(stats);

	nRemovedTuples = 0;

	cursor = grn_table_cursor_open(ctx, idsTable, NULL, 0, NULL, 0, 0, -1, 0);
	if (!cursor)
		ereport(ERROR,
				(errcode(ERRCODE_SYSTEM_ERROR),
				 errmsg("groonga: failed to open cursor: %s", ctx->errbuf)));

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
				GrnLock(index, ExclusiveLock);
				grn_table_cursor_delete(ctx, cursor);
				GrnUnlock(index, ExclusiveLock);

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

/**
 * pgroonga.vacuumcleanup() -- amvacuumcleanup
 */
Datum
pgroonga_vacuumcleanup(PG_FUNCTION_ARGS)
{
	IndexVacuumInfo *info = (IndexVacuumInfo *) PG_GETARG_POINTER(0);
	IndexBulkDeleteResult *stats = (IndexBulkDeleteResult *) PG_GETARG_POINTER(1);

	if (!stats)
		stats = GrnBulkDeleteResult(info,
									GrnLookupIDsTable(info->index, WARNING));

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
	return (Datum) 0;
}
