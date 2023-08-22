#include "pgroonga.h"

#include "pgrn-compatible.h"

#include "pgrn-global.h"
#include "pgrn-groonga.h"
#include "pgrn-options.h"
#include "pgrn-pg.h"
#include "pgrn-sequential-search.h"

#include <xxhash.h>

typedef struct PGrnSequentialSearchData
{
	grn_obj *table;
	grn_obj *textColumn;
	grn_obj *textsColumn;
	grn_obj *targetColumn;
	grn_id recordID;
	Oid indexOID;
	grn_obj *lexicon;
	grn_obj *indexColumn;
	grn_obj *indexColumnSource;
	grn_obj *matched;
	PGrnSequentialSearchType type;
	XXH64_hash_t expressionHash;
	grn_obj *expression;
	grn_obj *variable;
	bool useIndex;
	grn_expr_flags exprFlags;
} PGrnSequentialSearchData;

static grn_ctx *ctx = &PGrnContext;
static struct PGrnBuffers *buffers = &PGrnBuffers;
static PGrnSequentialSearchData data;

void
PGrnInitializeSequentialSearchData(void)
{
	data.table = grn_table_create(ctx,
								  NULL, 0,
								  NULL,
								  GRN_OBJ_TABLE_NO_KEY,
								  NULL, NULL);
	data.textColumn = grn_column_create(ctx,
										data.table,
										"text", strlen("text"),
										NULL,
										GRN_OBJ_COLUMN_SCALAR,
										grn_ctx_at(ctx, GRN_DB_TEXT));
	data.textsColumn = grn_column_create(ctx,
										 data.table,
										 "texts", strlen("texts"),
										 NULL,
										 GRN_OBJ_COLUMN_VECTOR,
										 grn_ctx_at(ctx, GRN_DB_TEXT));
	data.targetColumn = NULL;
	data.recordID = grn_table_add(ctx,
								  data.table,
								  NULL, 0,
								  NULL);
	data.indexOID = InvalidOid;
	data.lexicon = NULL;
	data.indexColumn = NULL;
	data.indexColumnSource = NULL;
	data.matched =
		grn_table_create(ctx,
						 NULL, 0,
						 NULL,
						 GRN_OBJ_TABLE_HASH_KEY | GRN_OBJ_WITH_SUBREC,
						 data.table,
						 NULL);
	data.type = PGRN_SEQUENTIAL_SEARCH_UNKNOWN;
	data.expressionHash = 0;
	data.expression = NULL;
	data.variable = NULL;
	data.useIndex = false;
	data.exprFlags = PGRN_EXPR_QUERY_PARSE_FLAGS;
}

void
PGrnFinalizeSequentialSearchData(void)
{
	if (data.expression)
		grn_obj_close(ctx, data.expression);
	grn_obj_close(ctx, data.matched);
	if (data.indexColumn)
		grn_obj_remove(ctx, data.indexColumn);
	if (data.lexicon)
		grn_obj_close(ctx, data.lexicon);
	grn_obj_close(ctx, data.textsColumn);
	grn_obj_close(ctx, data.textColumn);
	grn_obj_close(ctx, data.table);
}

static void
PGrnSequentialSearchDataExecuteClearIndex(void)
{
	if (data.indexColumn)
	{
		grn_obj_remove(ctx, data.indexColumn);
		data.indexColumn = NULL;
	}
	if (data.lexicon)
	{
		grn_obj_close(ctx, data.lexicon);
		data.lexicon = NULL;
	}
	data.indexOID = InvalidOid;

	data.type = PGRN_SEQUENTIAL_SEARCH_UNKNOWN;
	data.expressionHash = 0;
	if (data.expression)
	{
		grn_obj_close(ctx, data.expression);
		data.expression = NULL;
	}

	data.useIndex = false;
	data.exprFlags = PGRN_EXPR_QUERY_PARSE_FLAGS;
}

static bool
PGrnSequentialSearchDataExecutePrepareIndex(const char *indexName,
											unsigned int indexNameSize,
											PGrnSequentialSearchType type)
{
	const char *tag = "[sequential-search][index]";
	Oid indexOID = InvalidOid;
	grn_column_flags indexFlags = GRN_OBJ_COLUMN_INDEX;

	if (indexNameSize > 0)
	{
		grn_obj *text = &(buffers->general);

		grn_obj_reinit(ctx, text, GRN_DB_TEXT, 0);
		GRN_TEXT_SET(ctx, text, indexName, indexNameSize);
		GRN_TEXT_PUTC(ctx, text, '\0');
		indexOID = PGrnPGIndexNameToID(GRN_TEXT_VALUE(text));
	}

	if (data.indexOID == indexOID &&
		data.indexColumnSource == data.targetColumn &&
		data.type == type)
	{
		return false;
	}

	PGrnSequentialSearchDataExecuteClearIndex();
	data.useIndex = (PGrnIsTemporaryIndexSearchAvailable &&
					 (OidIsValid(indexOID) ||
					  data.targetColumn == data.textsColumn));

	if (grn_obj_is_vector_column(ctx, data.targetColumn))
		indexFlags |= GRN_OBJ_WITH_SECTION;

	if (data.indexOID != indexOID)
	{
		Relation index;
		bool isPGroongaIndex;

		index = PGrnPGResolveIndexID(indexOID);
		isPGroongaIndex = PGrnIndexIsPGroonga(index);
		if (!isPGroongaIndex) {
			RelationClose(index);
			PGrnCheckRC(GRN_INVALID_ARGUMENT,
						"%s[invalid] not PGroonga index: <%.*s>",
						tag,
						indexNameSize, indexName);
		}

		PG_TRY();
		{
			data.lexicon = PGrnCreateSimilarTemporaryLexicon(index, tag);
			data.exprFlags |= PGrnOptionsGetExprParseFlags(index);
		}
		PG_CATCH();
		{
			RelationClose(index);
			PG_RE_THROW();
		}
		PG_END_TRY();
		RelationClose(index);

		if (grn_obj_get_info(ctx, data.lexicon, GRN_INFO_DEFAULT_TOKENIZER, NULL))
			indexFlags |= GRN_OBJ_WITH_POSITION;
		data.indexColumn =
			PGrnCreateColumn(InvalidRelation,
							 data.lexicon,
							 "index",
							 indexFlags,
							 data.table);
		data.indexOID = indexOID;
	}

	if (!data.lexicon)
	{
		grn_obj *tokenizer = NULL;
		grn_obj *normalizers = &(buffers->normalizers);
		grn_table_flags tableFlags = GRN_OBJ_TABLE_PAT_KEY;

		switch (type)
		{
		case PGRN_SEQUENTIAL_SEARCH_EQUAL_QUERY:
			tokenizer = NULL;
			break;
		default:
			tokenizer = PGrnLookup(PGRN_DEFAULT_TOKENIZER, ERROR);
			indexFlags |= GRN_OBJ_WITH_POSITION;
			break;
		}
		GRN_TEXT_SETS(ctx, normalizers, PGRN_DEFAULT_NORMALIZERS);
		data.lexicon = PGrnCreateTable(InvalidRelation,
										NULL,
										tableFlags,
										grn_ctx_at(ctx, GRN_DB_SHORT_TEXT),
										tokenizer,
										normalizers,
										NULL);
		data.indexColumn =
			PGrnCreateColumn(InvalidRelation,
							 data.lexicon,
							 "index",
							 indexFlags,
							 data.table);
	}

	if (data.useIndex)
	{
		PGrnIndexColumnSetSource(InvalidRelation,
								 data.indexColumn,
								 data.targetColumn);
	}
	else
	{
		PGrnIndexColumnClearSources(InvalidRelation, data.indexColumn);
	}
	data.indexColumnSource = data.targetColumn;

	return true;
}

static void
PGrnSequentialSearchDataPrepare(grn_obj *column,
								grn_obj *value)
{
	grn_obj_set_value(ctx,
					  column,
					  data.recordID,
					  value,
					  GRN_OBJ_SET);
	data.targetColumn = column;
}

void
PGrnSequentialSearchDataPrepareText(const char *target,
									unsigned int targetSize)
{
	grn_obj *text = &(buffers->text);
	GRN_TEXT_SET(ctx, text, target, targetSize);
	PGrnSequentialSearchDataPrepare(data.textColumn,
									text);
}

void
PGrnSequentialSearchDataPrepareTexts(ArrayType *targets,
									 grn_obj *isTargets)
{
	grn_obj *texts = &(buffers->texts);
	ArrayIterator iterator;
	int i;
	int nTargets = 0;
	Datum datum;
	bool isNULL;

	GRN_BULK_REWIND(texts);
	iterator = pgrn_array_create_iterator(targets, 0);
	if (isTargets)
		nTargets = GRN_BULK_VSIZE(isTargets) / sizeof(grn_bool);
	for (i = 0; array_iterate(iterator, &datum, &isNULL); i++)
	{
		const char *target = NULL;
		unsigned int targetSize = 0;
		unsigned int weight  = 0;
		grn_id domain = GRN_DB_TEXT;

		if (nTargets > i && !GRN_BOOL_VALUE_AT(isTargets, i))
			continue;

		if (isNULL)
			continue;

		PGrnPGDatumExtractString(datum,
								 ARR_ELEMTYPE(targets),
								 &target,
								 &targetSize);
		if (!target)
			continue;

		grn_vector_add_element(ctx,
							   texts,
							   target,
							   targetSize,
							   weight,
							   domain);
	}
	array_free_iterator(iterator);

	PGrnSequentialSearchDataPrepare(data.textsColumn,
									texts);
}

static bool
PGrnSequentialSearchDataPrepareExpression(const char *expressionData,
										  unsigned int expressionDataSize,
										  const char *indexName,
										  unsigned int indexNameSize,
										  PGrnSequentialSearchType type)
{
	const char *tag = "[sequential-search][expression]";
	bool indexUpdated;
	XXH64_hash_t expressionHash;

	indexUpdated = PGrnSequentialSearchDataExecutePrepareIndex(indexName,
															   indexNameSize,
															   type);

	if (data.type != type)
	{
		data.expressionHash = 0;
		data.type = type;
	}

	expressionHash = XXH3_64bits(expressionData, expressionDataSize);
	if (!indexUpdated && data.expressionHash == expressionHash)
		return true;

	if (data.expression)
	{
		grn_obj_close(ctx, data.expression);
		data.expression = NULL;
	}

	GRN_EXPR_CREATE_FOR_QUERY(ctx,
							  data.table,
							  data.expression,
							  data.variable);
	if (!data.expression)
	{
		PGrnCheckRC(GRN_NO_MEMORY_AVAILABLE,
					"%s failed to create expression",
					tag);
	}

	data.expressionHash = expressionHash;

	return false;
}

void
PGrnSequentialSearchDataSetMatchTerm(const char *term,
									 unsigned int termSize,
									 const char *indexName,
									 unsigned int indexNameSize)
{
	const char *tag = "[sequential-search][match-term]";

	if (PGrnSequentialSearchDataPrepareExpression(term,
												  termSize,
												  indexName,
												  indexNameSize,
												  PGRN_SEQUENTIAL_SEARCH_MATCH_TERM))
	{
		return;
	}

	grn_expr_append_obj(ctx,
						data.expression,
						data.targetColumn,
						GRN_OP_GET_VALUE,
						1);
	PGrnCheck("%s append match target column", tag);
	grn_expr_append_const_str(ctx,
							  data.expression,
							  term,
							  termSize,
							  GRN_OP_PUSH,
							  1);
	PGrnCheck("%s append term to be matched", tag);
	grn_expr_append_op(ctx,
					   data.expression,
					   GRN_OP_MATCH,
					   2);
	PGrnCheck("%s append match operator", tag);
}

void
PGrnSequentialSearchDataSetEqualText(const char *other,
									 unsigned int otherSize,
									 const char *indexName,
									 unsigned int indexNameSize)
{
	const char *tag = "[sequential-search][equal-text]";

	if (PGrnSequentialSearchDataPrepareExpression(other,
												  otherSize,
												  indexName,
												  indexNameSize,
												  PGRN_SEQUENTIAL_SEARCH_EQUAL_TEXT))
	{
		return;
	}

	grn_expr_append_obj(ctx,
						data.expression,
						data.targetColumn,
						GRN_OP_GET_VALUE,
						1);
	PGrnCheck("%s append match target column", tag);
	grn_expr_append_const_str(ctx,
							  data.expression,
							  other,
							  otherSize,
							  GRN_OP_PUSH,
							  1);
	PGrnCheck("%s append equal text", tag);
	grn_expr_append_op(ctx,
					   data.expression,
					   GRN_OP_EQUAL,
					   2);
	PGrnCheck("%s append equal operator", tag);
}

void
PGrnSequentialSearchDataSetPrefix(const char *prefix,
								  unsigned int prefixSize,
								  const char *indexName,
								  unsigned int indexNameSize)
{
	const char *tag = "[sequential-search][prefix]";

	if (PGrnSequentialSearchDataPrepareExpression(prefix,
												  prefixSize,
												  indexName,
												  indexNameSize,
												  PGRN_SEQUENTIAL_SEARCH_PREFIX))
	{
		return;
	}

	grn_expr_append_obj(ctx,
						data.expression,
						data.targetColumn,
						GRN_OP_GET_VALUE,
						1);
	PGrnCheck("%s append match target column", tag);
	grn_expr_append_const_str(ctx,
							  data.expression,
							  prefix,
							  prefixSize,
							  GRN_OP_PUSH,
							  1);
	PGrnCheck("%s append prefix", tag);
	grn_expr_append_op(ctx,
					   data.expression,
					   GRN_OP_PREFIX,
					   2);
	PGrnCheck("%s append prefix operator", tag);
}

void
PGrnSequentialSearchDataSetQuery(const char *query,
								 unsigned int querySize,
								 const char *indexName,
								 unsigned int indexNameSize,
								 PGrnSequentialSearchType type)
{
	const char *tag = "[sequential-search][query]";

	if (PGrnSequentialSearchDataPrepareExpression(query,
												  querySize,
												  indexName,
												  indexNameSize,
												  type))
	{
		return;
	}

	grn_expr_parse(ctx,
				   data.expression,
				   query,
				   querySize,
				   data.targetColumn,
				   GRN_OP_MATCH,
				   GRN_OP_AND,
				   data.exprFlags);
	if (ctx->rc != GRN_SUCCESS)
		data.expressionHash = 0;
	PGrnCheck("%s failed to parse expression: <%.*s>",
			  tag,
			  (int) querySize, query);
}

void
PGrnSequentialSearchDataSetScript(const char *script,
								  unsigned int scriptSize,
								  const char *indexName,
								  unsigned int indexNameSize)
{
	const char *tag = "[sequential-search][query]";
	grn_expr_flags flags = GRN_EXPR_SYNTAX_SCRIPT;

	if (PGrnSequentialSearchDataPrepareExpression(script,
												  scriptSize,
												  indexName,
												  indexNameSize,
												  PGRN_SEQUENTIAL_SEARCH_SCRIPT))
	{
		return;
	}

	grn_expr_parse(ctx,
				   data.expression,
				   script, scriptSize,
				   data.targetColumn,
				   GRN_OP_MATCH, GRN_OP_AND,
				   flags);
	if (ctx->rc != GRN_SUCCESS)
		data.expressionHash = 0;
	PGrnCheck("%s failed to parse expression: <%.*s>",
			  tag,
			  (int) scriptSize, script);
}

bool
PGrnSequentialSearchDataExecute(void)
{
	bool matched = false;

	if (data.useIndex)
	{
		grn_table_select(ctx,
						 data.table,
						 data.expression,
						 data.matched,
						 GRN_OP_OR);

		if (grn_table_size(ctx, data.matched) == 1)
		{
			matched = true;
			grn_table_delete(ctx,
							 data.matched,
							 &(data.recordID),
							 sizeof(grn_id));
		}
	}
	else
	{
		grn_obj *result;

		GRN_RECORD_SET(ctx, data.variable, data.recordID);
		result = grn_expr_exec(ctx, data.expression, 0);
		GRN_OBJ_IS_TRUE(ctx, result, matched);
	}

	return matched;
}
