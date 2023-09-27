#include "pgroonga.h"

#include "pgrn-compatible.h"

#include "pgrn-global.h"
#include "pgrn-groonga.h"
#include "pgrn-options.h"
#include "pgrn-pg.h"
#include "pgrn-sequential-search.h"

#include <xxhash.h>

typedef enum {
	PGRN_SEQUENTIAL_SEARCH_TARGET_TEXT,
	PGRN_SEQUENTIAL_SEARCH_TARGET_TEXTS,
} PGrnSequentialSearchTargetType;

typedef struct PGrnSequentialSearchDatumKey
{
	Oid indexOID;
	PGrnSequentialSearchTargetType targetType;
	bool useIndex;
	PGrnSequentialSearchType type;
} PGrnSequentialSearchDatumKey;

typedef struct PGrnSequentialSearchDatum
{
	grn_obj *table;
	PGrnSequentialSearchTargetType targetType;
	grn_obj *targetColumn;
	grn_id recordID;
	Oid indexOID;
	grn_obj *lexicon;
	grn_obj *indexColumn;
	grn_obj *matched;
	PGrnSequentialSearchType type;
	XXH64_hash_t expressionHash;
	grn_obj *expression;
	grn_obj *variable;
	bool useIndex;
	grn_expr_flags exprFlags;
	bool used;
} PGrnSequentialSearchDatum;

static grn_ctx *ctx = &PGrnContext;
static struct PGrnBuffers *buffers = &PGrnBuffers;
static PGrnSequentialSearchTargetType currentTargetType =
	PGRN_SEQUENTIAL_SEARCH_TARGET_TEXT;
static PGrnSequentialSearchDatum *currentDatum = NULL;
static grn_hash *data = NULL;
static uint32_t nExecutions = 0;
/* Remove unused PGrnSequentialSearchDatum per 100 executions. */
static const uint32_t vacuumFrequency = 100;

static void
PGrnSequentialSearchDatumInitialize(PGrnSequentialSearchDatum *datum)
{
	grn_column_flags flags = 0;
	datum->table = grn_table_create(ctx,
									NULL, 0,
									NULL,
									GRN_OBJ_TABLE_NO_KEY,
									NULL, NULL);
	datum->targetType = currentTargetType;
	if (datum->targetType == PGRN_SEQUENTIAL_SEARCH_TARGET_TEXT)
	{
		flags = GRN_OBJ_COLUMN_SCALAR;
	}
	else
	{
		flags = GRN_OBJ_COLUMN_VECTOR;
	}
	datum->targetColumn = grn_column_create(ctx,
											datum->table,
											"target", strlen("target"),
											NULL,
											flags,
											grn_ctx_at(ctx, GRN_DB_TEXT));
	datum->recordID = grn_table_add(ctx,
									datum->table,
									NULL, 0,
									NULL);
	datum->indexOID = InvalidOid;
	datum->lexicon = NULL;
	datum->indexColumn = NULL;
	datum->matched =
		grn_table_create(ctx,
						 NULL, 0,
						 NULL,
						 GRN_OBJ_TABLE_HASH_KEY | GRN_OBJ_WITH_SUBREC,
						 datum->table,
						 NULL);
	datum->type = PGRN_SEQUENTIAL_SEARCH_UNKNOWN;
	datum->expressionHash = 0;
	datum->expression = NULL;
	datum->variable = NULL;
	datum->useIndex = false;
	datum->exprFlags = PGRN_EXPR_QUERY_PARSE_FLAGS;
	datum->used = true;
}

static void
PGrnSequentialSearchDatumFinalize(PGrnSequentialSearchDatum *datum)
{
	if (datum->expression)
		grn_obj_close(ctx, datum->expression);
	grn_obj_close(ctx, datum->matched);
	if (datum->indexColumn)
		grn_obj_remove(ctx, datum->indexColumn);
	if (datum->lexicon)
		grn_obj_close(ctx, datum->lexicon);
	grn_obj_close(ctx, datum->targetColumn);
	grn_obj_close(ctx, datum->table);
}

void
PGrnInitializeSequentialSearch(void)
{
	currentTargetType = PGRN_SEQUENTIAL_SEARCH_TARGET_TEXT;
	currentDatum = NULL;
	data = grn_hash_create(ctx,
						   NULL,
						   sizeof(PGrnSequentialSearchDatumKey),
						   sizeof(PGrnSequentialSearchDatum),
						   GRN_TABLE_HASH_KEY);
}

void
PGrnFinalizeSequentialSearch(void)
{
	GRN_HASH_EACH_BEGIN(ctx, data, cursor, id) {
		void *value;
		PGrnSequentialSearchDatum *datum;
		grn_hash_cursor_get_value(ctx, cursor, &value);
		datum = value;
		PGrnSequentialSearchDatumFinalize(datum);
	} GRN_HASH_EACH_END(ctx, cursor);
	grn_hash_close(ctx, data);
}

void
PGrnReleaseSequentialSearch(ResourceReleasePhase phase,
							bool isCommit,
							bool isTopLevel,
							void *arg)
{
	const char *tag = "pgroonga: [release][sequential-search]";

	if (!(isTopLevel && phase == RESOURCE_RELEASE_AFTER_LOCKS))
	{
		return;
	}

	nExecutions++;
	if ((nExecutions % vacuumFrequency) != 0)
	{
		return;
	}

	GRN_LOG(ctx, GRN_LOG_DEBUG, "%s[start] %u", tag, grn_hash_size(ctx, data));
	GRN_HASH_EACH_BEGIN(ctx, data, cursor, id) {
		void *value;
		PGrnSequentialSearchDatum *datum;
		grn_hash_cursor_get_value(ctx, cursor, &value);
		datum = value;
		if (datum->used)
		{
			datum->used = false;
		}
		else
		{
			PGrnSequentialSearchDatumFinalize(datum);
			grn_hash_cursor_delete(ctx, cursor, NULL);
		}
	} GRN_HASH_EACH_END(ctx, cursor);
	GRN_LOG(ctx, GRN_LOG_DEBUG, "%s[end] %u", tag, grn_hash_size(ctx, data));
}

void
PGrnSequentialSearchSetTargetText(const char *target,
								  unsigned int targetSize)
{
	grn_obj *text = &(buffers->text);
	GRN_TEXT_SET(ctx, text, target, targetSize);
	currentTargetType = PGRN_SEQUENTIAL_SEARCH_TARGET_TEXT;
}

void
PGrnSequentialSearchSetTargetTexts(ArrayType *targets,
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

	currentTargetType = PGRN_SEQUENTIAL_SEARCH_TARGET_TEXTS;
}

static bool
PGrnSequentialSearchPrepareIndex(const char *indexName,
								 unsigned int indexNameSize,
								 PGrnSequentialSearchType type)
{
	const char *tag = "[sequential-search][index]";
	Oid indexOID = InvalidOid;
	grn_column_flags indexFlags = GRN_OBJ_COLUMN_INDEX;
	bool targetIsVector =
		(currentTargetType == PGRN_SEQUENTIAL_SEARCH_TARGET_TEXTS);
	PGrnSequentialSearchDatumKey key;

	if (indexNameSize > 0)
	{
		grn_obj *text = &(buffers->general);

		grn_obj_reinit(ctx, text, GRN_DB_TEXT, 0);
		GRN_TEXT_SET(ctx, text, indexName, indexNameSize);
		GRN_TEXT_PUTC(ctx, text, '\0');
		indexOID = PGrnPGIndexNameToID(GRN_TEXT_VALUE(text));
	}

	key.indexOID = indexOID;
	key.targetType = currentTargetType;
	key.useIndex = (PGrnIsTemporaryIndexSearchAvailable &&
					(OidIsValid(indexOID) || targetIsVector));
	key.type = type;
	if (currentDatum &&
		currentDatum->indexOID == key.indexOID &&
		currentDatum->targetType == key.targetType &&
		currentDatum->useIndex == key.useIndex &&
		currentDatum->type == key.type)
	{
		return false;
	}

	{
		grn_id id;
		void *value;
		id = grn_hash_get(ctx, data, &key, sizeof(key), &value);
		if (id != GRN_ID_NIL)
		{
			currentDatum = value;
			currentDatum->used = true;
			return false;
		}

		id = grn_hash_add(ctx, data, &key, sizeof(key), &value, NULL);
		currentDatum = value;
		PGrnSequentialSearchDatumInitialize(currentDatum);
	}

	currentDatum->indexOID = key.indexOID;
	currentDatum->useIndex = key.useIndex;
	if (!currentDatum->useIndex)
	{
		return true;
	}

	if (targetIsVector)
		indexFlags |= GRN_OBJ_WITH_SECTION;

	if (OidIsValid(currentDatum->indexOID))
	{
		Relation index;
		bool isPGroongaIndex;

		index = PGrnPGResolveIndexID(currentDatum->indexOID);
		isPGroongaIndex = PGrnIndexIsPGroonga(index);
		if (!isPGroongaIndex) {
			PGrnSequentialSearchDatumFinalize(currentDatum);
			grn_hash_delete(ctx, data, &key, sizeof(key), NULL);
			currentDatum = NULL;
			RelationClose(index);
			PGrnCheckRC(GRN_INVALID_ARGUMENT,
						"%s[invalid] not PGroonga index: <%.*s>",
						tag,
						indexNameSize, indexName);
		}

		PG_TRY();
		{
			currentDatum->lexicon = PGrnCreateSimilarTemporaryLexicon(index, tag);
			currentDatum->exprFlags |= PGrnOptionsGetExprParseFlags(index);
		}
		PG_CATCH();
		{
			PGrnSequentialSearchDatumFinalize(currentDatum);
			grn_hash_delete(ctx, data, &key, sizeof(key), NULL);
			currentDatum = NULL;
			RelationClose(index);
			PG_RE_THROW();
		}
		PG_END_TRY();
		RelationClose(index);

		if (grn_obj_get_info(ctx,
							 currentDatum->lexicon,
							 GRN_INFO_DEFAULT_TOKENIZER,
							 NULL))
		{
			indexFlags |= GRN_OBJ_WITH_POSITION;
		}

		PG_TRY();
		{
			currentDatum->indexColumn =
				PGrnCreateColumn(InvalidRelation,
								 currentDatum->lexicon,
								 "index",
								 indexFlags,
								 currentDatum->table);
			PGrnIndexColumnSetSource(InvalidRelation,
									 currentDatum->indexColumn,
									 currentDatum->targetColumn);
		}
		PG_CATCH();
		{
			PGrnSequentialSearchDatumFinalize(currentDatum);
			grn_hash_delete(ctx, data, &key, sizeof(key), NULL);
			currentDatum = NULL;
			PG_RE_THROW();
		}
		PG_END_TRY();
	}
	else
	{
		PG_TRY();
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
			currentDatum->lexicon =
				PGrnCreateTable(InvalidRelation,
								NULL,
								tableFlags,
								grn_ctx_at(ctx, GRN_DB_SHORT_TEXT),
								tokenizer,
								normalizers,
								NULL);
			currentDatum->indexColumn =
				PGrnCreateColumn(InvalidRelation,
								 currentDatum->lexicon,
								 "index",
								 indexFlags,
								 currentDatum->table);
			PGrnIndexColumnSetSource(InvalidRelation,
									 currentDatum->indexColumn,
									 currentDatum->targetColumn);
		}
		PG_CATCH();
		{
			PGrnSequentialSearchDatumFinalize(currentDatum);
			grn_hash_delete(ctx, data, &key, sizeof(key), NULL);
			currentDatum = NULL;
			PG_RE_THROW();
		}
		PG_END_TRY();
	}

	return true;
}

static bool
PGrnSequentialSearchPrepareExpression(const char *expressionData,
									  unsigned int expressionDataSize,
									  const char *indexName,
									  unsigned int indexNameSize,
									  PGrnSequentialSearchType type)
{
	const char *tag = "[sequential-search][expression]";
	bool indexUpdated;
	XXH64_hash_t expressionHash;

	indexUpdated = PGrnSequentialSearchPrepareIndex(indexName,
													indexNameSize,
													type);
	expressionHash = XXH3_64bits(expressionData, expressionDataSize);
	if (!indexUpdated && currentDatum->expressionHash == expressionHash)
		return true;

	if (currentDatum->expression)
	{
		grn_obj_close(ctx, currentDatum->expression);
		currentDatum->expression = NULL;
	}

	GRN_EXPR_CREATE_FOR_QUERY(ctx,
							  currentDatum->table,
							  currentDatum->expression,
							  currentDatum->variable);
	if (!currentDatum->expression)
	{
		PGrnCheckRC(GRN_NO_MEMORY_AVAILABLE,
					"%s failed to create expression",
					tag);
	}

	currentDatum->expressionHash = expressionHash;

	return false;
}

void
PGrnSequentialSearchSetMatchTerm(const char *term,
								 unsigned int termSize,
								 const char *indexName,
								 unsigned int indexNameSize)
{
	const char *tag = "[sequential-search][match-term]";

	if (PGrnSequentialSearchPrepareExpression(term,
											  termSize,
											  indexName,
											  indexNameSize,
											  PGRN_SEQUENTIAL_SEARCH_MATCH_TERM))
	{
		return;
	}

	grn_expr_append_obj(ctx,
						currentDatum->expression,
						currentDatum->targetColumn,
						GRN_OP_GET_VALUE,
						1);
	PGrnCheck("%s append match target column", tag);
	grn_expr_append_const_str(ctx,
							  currentDatum->expression,
							  term,
							  termSize,
							  GRN_OP_PUSH,
							  1);
	PGrnCheck("%s append term to be matched", tag);
	grn_expr_append_op(ctx,
					   currentDatum->expression,
					   GRN_OP_MATCH,
					   2);
	PGrnCheck("%s append match operator", tag);
}

void
PGrnSequentialSearchSetEqualText(const char *other,
								 unsigned int otherSize,
								 const char *indexName,
								 unsigned int indexNameSize)
{
	const char *tag = "[sequential-search][equal-text]";

	if (PGrnSequentialSearchPrepareExpression(other,
											  otherSize,
											  indexName,
											  indexNameSize,
											  PGRN_SEQUENTIAL_SEARCH_EQUAL_TEXT))
	{
		return;
	}

	grn_expr_append_obj(ctx,
						currentDatum->expression,
						currentDatum->targetColumn,
						GRN_OP_GET_VALUE,
						1);
	PGrnCheck("%s append match target column", tag);
	grn_expr_append_const_str(ctx,
							  currentDatum->expression,
							  other,
							  otherSize,
							  GRN_OP_PUSH,
							  1);
	PGrnCheck("%s append equal text", tag);
	grn_expr_append_op(ctx,
					   currentDatum->expression,
					   GRN_OP_EQUAL,
					   2);
	PGrnCheck("%s append equal operator", tag);
}

void
PGrnSequentialSearchSetPrefix(const char *prefix,
							  unsigned int prefixSize,
							  const char *indexName,
							  unsigned int indexNameSize)
{
	const char *tag = "[sequential-search][prefix]";

	if (PGrnSequentialSearchPrepareExpression(prefix,
											  prefixSize,
											  indexName,
											  indexNameSize,
											  PGRN_SEQUENTIAL_SEARCH_PREFIX))
	{
		return;
	}

	grn_expr_append_obj(ctx,
						currentDatum->expression,
						currentDatum->targetColumn,
						GRN_OP_GET_VALUE,
						1);
	PGrnCheck("%s append match target column", tag);
	grn_expr_append_const_str(ctx,
							  currentDatum->expression,
							  prefix,
							  prefixSize,
							  GRN_OP_PUSH,
							  1);
	PGrnCheck("%s append prefix", tag);
	grn_expr_append_op(ctx,
					   currentDatum->expression,
					   GRN_OP_PREFIX,
					   2);
	PGrnCheck("%s append prefix operator", tag);
}

void
PGrnSequentialSearchSetQuery(const char *query,
							 unsigned int querySize,
							 const char *indexName,
							 unsigned int indexNameSize,
							 PGrnSequentialSearchType type)
{
	const char *tag = "[sequential-search][query]";

	if (PGrnSequentialSearchPrepareExpression(query,
											  querySize,
											  indexName,
											  indexNameSize,
											  type))
	{
		return;
	}

	grn_expr_parse(ctx,
				   currentDatum->expression,
				   query,
				   querySize,
				   currentDatum->targetColumn,
				   GRN_OP_MATCH,
				   GRN_OP_AND,
				   currentDatum->exprFlags);
	if (ctx->rc != GRN_SUCCESS)
		currentDatum->expressionHash = 0;
	PGrnCheck("%s failed to parse expression: <%.*s>",
			  tag,
			  (int) querySize, query);
}

void
PGrnSequentialSearchSetScript(const char *script,
							  unsigned int scriptSize,
							  const char *indexName,
							  unsigned int indexNameSize)
{
	const char *tag = "[sequential-search][query]";
	grn_expr_flags flags = GRN_EXPR_SYNTAX_SCRIPT;

	if (PGrnSequentialSearchPrepareExpression(script,
											  scriptSize,
											  indexName,
											  indexNameSize,
											  PGRN_SEQUENTIAL_SEARCH_SCRIPT))
	{
		return;
	}

	grn_expr_parse(ctx,
				   currentDatum->expression,
				   script, scriptSize,
				   currentDatum->targetColumn,
				   GRN_OP_MATCH, GRN_OP_AND,
				   flags);
	if (ctx->rc != GRN_SUCCESS)
		currentDatum->expressionHash = 0;
	PGrnCheck("%s failed to parse expression: <%.*s>",
			  tag,
			  (int) scriptSize, script);
}

bool
PGrnSequentialSearchExecute(void)
{
	bool matched = false;
	grn_obj *value;

	if (currentTargetType == PGRN_SEQUENTIAL_SEARCH_TARGET_TEXT)
	{
		value = &(buffers->text);
	}
	else
	{
		value = &(buffers->texts);
	}
	grn_obj_set_value(ctx,
					  currentDatum->targetColumn,
					  currentDatum->recordID,
					  value,
					  GRN_OBJ_SET);

	if (currentDatum->useIndex)
	{
		grn_table_select(ctx,
						 currentDatum->table,
						 currentDatum->expression,
						 currentDatum->matched,
						 GRN_OP_OR);

		if (grn_table_size(ctx, currentDatum->matched) == 1)
		{
			matched = true;
			grn_table_delete(ctx,
							 currentDatum->matched,
							 &(currentDatum->recordID),
							 sizeof(grn_id));
		}
	}
	else
	{
		grn_obj *result;

		GRN_RECORD_SET(ctx, currentDatum->variable, currentDatum->recordID);
		result = grn_expr_exec(ctx, currentDatum->expression, 0);
		GRN_OBJ_IS_TRUE(ctx, result, matched);
	}

	return matched;
}
