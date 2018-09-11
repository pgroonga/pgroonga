#include "pgroonga.h"

#include "pgrn-compatible.h"

#include "pgrn-global.h"
#include "pgrn-groonga.h"
#include "pgrn-options.h"
#include "pgrn-pg.h"
#include "pgrn-sequential-search.h"

#include <xxhash.h>

static grn_ctx *ctx = &PGrnContext;
static struct PGrnBuffers *buffers = &PGrnBuffers;

void
PGrnSequentialSearchDataInitialize(PGrnSequentialSearchData *data)
{
	data->table = grn_table_create(ctx,
								   NULL, 0,
								   NULL,
								   GRN_OBJ_TABLE_NO_KEY,
								   NULL, NULL);
	data->textColumn = grn_column_create(ctx,
										 data->table,
										 "text", strlen("text"),
										 NULL,
										 GRN_OBJ_COLUMN_SCALAR,
										 grn_ctx_at(ctx, GRN_DB_TEXT));
	data->textsColumn = grn_column_create(ctx,
										  data->table,
										  "texts", strlen("texts"),
										  NULL,
										  GRN_OBJ_COLUMN_VECTOR,
										  grn_ctx_at(ctx, GRN_DB_TEXT));
	data->recordID = grn_table_add(ctx,
								   data->table,
								   NULL, 0,
								   NULL);
	data->indexOID = InvalidOid;
	data->lexicon = NULL;
	data->indexColumn = NULL;
	data->indexColumnSource = NULL;
	data->matched =
		grn_table_create(ctx,
						 NULL, 0,
						 NULL,
						 GRN_OBJ_TABLE_HASH_KEY | GRN_OBJ_WITH_SUBREC,
						 data->table,
						 NULL);
	data->type = PGRN_SEQUENTIAL_SEARCH_UNKNOWN;
	data->expressionHash = 0;
	data->expression = NULL;
	data->variable = NULL;
	data->useIndex = false;
	data->exprFlags = PGRN_EXPR_QUERY_PARSE_FLAGS;
}

void
PGrnSequentialSearchDataFinalize(PGrnSequentialSearchData *data)
{
	if (data->expression)
		grn_obj_close(ctx, data->expression);
	grn_obj_close(ctx, data->matched);
	if (data->indexColumn)
		grn_obj_remove(ctx, data->indexColumn);
	if (data->lexicon)
		grn_obj_close(ctx, data->lexicon);
	grn_obj_close(ctx, data->textsColumn);
	grn_obj_close(ctx, data->textColumn);
	grn_obj_close(ctx, data->table);
}

static void
PGrnSequentialSearchDataExecutePrepareIndex(PGrnSequentialSearchData *data,
											Oid indexOID,
											grn_obj *source)
{
	if (data->indexOID == indexOID)
	{
		if (data->indexColumnSource != source)
		{
			if (data->indexColumn)
				PGrnIndexColumnClearSources(InvalidRelation, data->indexColumn);
			data->indexColumnSource = NULL;
		}
		return;
	}

	if (data->indexColumn)
	{
		grn_obj_remove(ctx, data->indexColumn);
		data->indexColumn = NULL;
	}
	if (data->lexicon)
	{
		grn_obj_close(ctx, data->lexicon);
		data->lexicon = NULL;
	}
	data->indexOID = InvalidOid;

	data->type = PGRN_SEQUENTIAL_SEARCH_UNKNOWN;
	data->expressionHash = 0;
	if (data->expression)
	{
		grn_obj_close(ctx, data->expression);
		data->expression = NULL;
	}

	data->indexColumnSource = NULL;
	data->useIndex = false;
	data->exprFlags = PGRN_EXPR_QUERY_PARSE_FLAGS;
}

static void
PGrnSequentialSearchDataExecuteSetIndex(PGrnSequentialSearchData *data,
										Oid indexOID,
										grn_obj *source,
										const char *indexName,
										unsigned int indexNameSize)
{
	grn_column_flags column_flags =
		GRN_OBJ_COLUMN_INDEX |
		GRN_OBJ_WITH_POSITION;

	data->useIndex = (PGrnIsTemporaryIndexSearchAvailable &&
					  (OidIsValid(indexOID) ||
					   source == data->textsColumn));

	if (grn_obj_is_vector_column(ctx, source))
		column_flags |= GRN_OBJ_WITH_SECTION;

	if (data->indexOID != indexOID)
	{
		Relation index;
		bool isPGroongaIndex;
		grn_obj *tokenizer = NULL;
		grn_obj *normalizer = NULL;
		grn_obj *tokenFilters = &(buffers->tokenFilters);
		grn_table_flags table_flags = 0;

		index = PGrnPGResolveIndexID(indexOID);
		isPGroongaIndex = PGrnIndexIsPGroonga(index);
		if (!isPGroongaIndex) {
			RelationClose(index);
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("pgroonga: [sequential-search][index][invalid] "
							"not PGroonga index: <%.*s>",
							indexNameSize,
							indexName)));
		}

		GRN_BULK_REWIND(tokenFilters);
		PGrnApplyOptionValues(index,
							  PGRN_OPTION_USE_CASE_FULL_TEXT_SEARCH,
							  &tokenizer, PGRN_DEFAULT_TOKENIZER,
							  &normalizer, PGRN_DEFAULT_NORMALIZER,
							  tokenFilters,
							  &table_flags);
		data->exprFlags |= PGrnOptionsGetExprParseFlags(index);
		RelationClose(index);

		data->lexicon = PGrnCreateTable(InvalidRelation,
										NULL,
										table_flags,
										grn_ctx_at(ctx, GRN_DB_SHORT_TEXT),
										tokenizer,
										normalizer,
										tokenFilters);
		data->indexColumn =
			PGrnCreateColumn(InvalidRelation,
							 data->lexicon,
							 "index",
							 column_flags,
							 data->table);
		data->indexOID = indexOID;
	}

	if (!data->lexicon)
	{
		grn_obj *tokenizer;
		grn_obj *normalizer;
		grn_table_flags table_flags = GRN_OBJ_TABLE_PAT_KEY;

		tokenizer = PGrnLookup(PGRN_DEFAULT_TOKENIZER, ERROR);
		normalizer = PGrnLookup(PGRN_DEFAULT_NORMALIZER, ERROR);
		data->lexicon = PGrnCreateTable(InvalidRelation,
										NULL,
										table_flags,
										grn_ctx_at(ctx, GRN_DB_SHORT_TEXT),
										tokenizer,
										normalizer,
										NULL);
		data->indexColumn =
			PGrnCreateColumn(InvalidRelation,
							 data->lexicon,
							 "index",
							 column_flags,
							 data->table);
	}

	if (data->indexColumnSource != source)
	{
		if (data->useIndex)
		{
			PGrnIndexColumnSetSource(InvalidRelation,
									 data->indexColumn,
									 source);
		}
		else
		{
			PGrnIndexColumnClearSources(InvalidRelation, data->indexColumn);
		}
		data->indexColumnSource = source;
	}
}

static void
PGrnSequentialSearchDataPrepare(PGrnSequentialSearchData *data,
								grn_obj *column,
								grn_obj *value,
								const char *indexName,
								unsigned int indexNameSize)
{
	Oid indexOID = InvalidOid;

	if (indexNameSize > 0)
	{
		grn_obj *text = &(buffers->general);

		grn_obj_reinit(ctx, text, GRN_DB_TEXT, 0);
		GRN_TEXT_SET(ctx, text, indexName, indexNameSize);
		GRN_TEXT_PUTC(ctx, text, '\0');
		indexOID = PGrnPGIndexNameToID(GRN_TEXT_VALUE(text));
	}
	PGrnSequentialSearchDataExecutePrepareIndex(data, indexOID, column);

	grn_obj_set_value(ctx,
					  column,
					  data->recordID,
					  value,
					  GRN_OBJ_SET);

	if (PGrnIsTemporaryIndexSearchAvailable)
	{
		PGrnSequentialSearchDataExecuteSetIndex(data,
												indexOID,
												column,
												indexName,
												indexNameSize);
	}
}

void
PGrnSequentialSearchDataPrepareText(PGrnSequentialSearchData *data,
									const char *target,
									unsigned int targetSize,
									const char *indexName,
									unsigned int indexNameSize)
{
	grn_obj *text = &(buffers->text);

	GRN_TEXT_SET(ctx, text, target, targetSize);
	PGrnSequentialSearchDataPrepare(data,
									data->textColumn,
									text,
									indexName,
									indexNameSize);
}

void
PGrnSequentialSearchDataPrepareTexts(PGrnSequentialSearchData *data,
									 ArrayType *targets,
									 grn_obj *isTargets,
									 const char *indexName,
									 unsigned int indexNameSize)
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

	PGrnSequentialSearchDataPrepare(data,
									data->textsColumn,
									texts,
									indexName,
									indexNameSize);
}

static bool
PGrnSequentialSearchDataPrepareExpression(PGrnSequentialSearchData *data,
										  const char *expressionData,
										  unsigned int expressionDataSize,
										  PGrnSequentialSearchType type)
{
	uint64_t expressionHash;

	if (data->type != type)
	{
		data->expressionHash = 0;
		data->type = type;
	}

	expressionHash = XXH64(expressionData, expressionDataSize, 0);
	if (data->expressionHash == expressionHash)
		return true;

	if (data->expression)
	{
		grn_obj_close(ctx, data->expression);
		data->expression = NULL;
	}

	GRN_EXPR_CREATE_FOR_QUERY(ctx,
							  data->table,
							  data->expression,
							  data->variable);
	if (!data->expression)
	{
		ereport(ERROR,
				(errcode(ERRCODE_OUT_OF_MEMORY),
				 errmsg("pgroonga: failed to create expression: %s",
						ctx->errbuf)));
	}

	data->expressionHash = expressionHash;

	return false;
}

void
PGrnSequentialSearchDataSetMatchTerm(PGrnSequentialSearchData *data,
									 const char *term,
									 unsigned int termSize)
{
	if (PGrnSequentialSearchDataPrepareExpression(data,
												  term,
												  termSize,
												  PGRN_SEQUENTIAL_SEARCH_MATCH_TERM))
	{
		return;
	}

	grn_expr_append_obj(ctx,
						data->expression,
						data->indexColumnSource,
						GRN_OP_GET_VALUE,
						1);
	PGrnCheck("match-term: append match target column");
	grn_expr_append_const_str(ctx,
							  data->expression,
							  term,
							  termSize,
							  GRN_OP_PUSH,
							  1);
	PGrnCheck("match-term: append term to be matched");
	grn_expr_append_op(ctx,
					   data->expression,
					   GRN_OP_MATCH,
					   2);
	PGrnCheck("match-term: append match operator");
}

void
PGrnSequentialSearchDataSetQuery(PGrnSequentialSearchData *data,
								 const char *query,
								 unsigned int querySize)
{
	grn_rc rc;

	if (PGrnSequentialSearchDataPrepareExpression(data,
												  query,
												  querySize,
												  PGRN_SEQUENTIAL_SEARCH_QUERY))
	{
		return;
	}

	rc = grn_expr_parse(ctx,
						data->expression,
						query, querySize,
						data->indexColumnSource,
						GRN_OP_MATCH, GRN_OP_AND,
						data->exprFlags);
	if (rc != GRN_SUCCESS)
	{
		char message[GRN_CTX_MSGSIZE];
		grn_strncpy(message, GRN_CTX_MSGSIZE,
					ctx->errbuf, GRN_CTX_MSGSIZE);

		data->expressionHash = 0;

		ereport(ERROR,
				(errcode(PGrnRCToPgErrorCode(rc)),
				 errmsg("pgroonga: failed to parse expression: %s",
						message)));
	}
}

void
PGrnSequentialSearchDataSetScript(PGrnSequentialSearchData *data,
								  const char *script,
								  unsigned int scriptSize)
{
	grn_expr_flags flags = GRN_EXPR_SYNTAX_SCRIPT;
	grn_rc rc;

	if (PGrnSequentialSearchDataPrepareExpression(data,
												  script,
												  scriptSize,
												  PGRN_SEQUENTIAL_SEARCH_SCRIPT))
	{
		return;
	}

	rc = grn_expr_parse(ctx,
						data->expression,
						script, scriptSize,
						data->indexColumnSource,
						GRN_OP_MATCH, GRN_OP_AND,
						flags);
	if (rc != GRN_SUCCESS)
	{
		char message[GRN_CTX_MSGSIZE];
		grn_strncpy(message, GRN_CTX_MSGSIZE,
					ctx->errbuf, GRN_CTX_MSGSIZE);

		data->expressionHash = 0;

		ereport(ERROR,
				(errcode(PGrnRCToPgErrorCode(rc)),
				 errmsg("pgroonga: failed to parse expression: %s",
						message)));
	}
}

bool
PGrnSequentialSearchDataExecute(PGrnSequentialSearchData *data)
{
	bool matched = false;

	if (data->useIndex)
	{
		grn_table_select(ctx,
						 data->table,
						 data->expression,
						 data->matched,
						 GRN_OP_OR);

		if (grn_table_size(ctx, data->matched) == 1)
		{
			matched = true;
			grn_table_delete(ctx,
							 data->matched,
							 &(data->recordID),
							 sizeof(grn_id));
		}
	}
	else
	{
		grn_obj *result;

		GRN_RECORD_SET(ctx, data->variable, data->recordID);
		result = grn_expr_exec(ctx, data->expression, 0);
		GRN_OBJ_IS_TRUE(ctx, result, matched);
	}

	return matched;
}
