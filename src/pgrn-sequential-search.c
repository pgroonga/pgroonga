#include "pgroonga.h"

#include "pgrn-global.h"
#include "pgrn-groonga.h"
#include "pgrn-options.h"
#include "pgrn-pg.h"
#include "pgrn-sequential-search.h"

#include <xxhash.h>

typedef enum
{
	PGRN_SEQUENTIAL_SEARCH_TARGET_TEXT,
	PGRN_SEQUENTIAL_SEARCH_TARGET_TEXTS,
} PGrnSequentialSearchTargetType;

typedef struct PGrnSequentialSearchDatumKey
{
	Oid indexOID;
	int attributeIndex;
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
	int attributeIndex;
	grn_obj *lexicon;
	grn_obj *indexColumn;
	grn_obj *matched;
	PGrnSequentialSearchType type;
	XXH64_hash_t expressionHash;
	grn_obj *expression;
	grn_obj *variable;
	bool useIndex;
	float4 fuzzyMaxDistanceRatio;
	grn_expr_flags exprFlags;
	bool used;
} PGrnSequentialSearchDatum;

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
	datum->table =
		grn_table_create(ctx, NULL, 0, NULL, GRN_OBJ_TABLE_NO_KEY, NULL, NULL);
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
											"target",
											strlen("target"),
											NULL,
											flags,
											grn_ctx_at(ctx, GRN_DB_TEXT));
	datum->recordID = grn_table_add(ctx, datum->table, NULL, 0, NULL);
	datum->indexOID = InvalidOid;
	datum->lexicon = NULL;
	datum->indexColumn = NULL;
	datum->matched =
		grn_table_create(ctx,
						 NULL,
						 0,
						 NULL,
						 GRN_OBJ_TABLE_HASH_KEY | GRN_OBJ_WITH_SUBREC,
						 datum->table,
						 NULL);
	datum->type = PGRN_SEQUENTIAL_SEARCH_UNKNOWN;
	datum->expressionHash = 0;
	datum->expression = NULL;
	datum->variable = NULL;
	datum->useIndex = false;
	datum->fuzzyMaxDistanceRatio = 0.0;
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
		grn_obj_close(ctx, datum->indexColumn);
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
	GRN_HASH_EACH_BEGIN(ctx, data, cursor, id)
	{
		void *value;
		PGrnSequentialSearchDatum *datum;
		grn_hash_cursor_get_value(ctx, cursor, &value);
		datum = value;
		PGrnSequentialSearchDatumFinalize(datum);
	}
	GRN_HASH_EACH_END(ctx, cursor);
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
	GRN_HASH_EACH_BEGIN(ctx, data, cursor, id)
	{
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
			if (datum == currentDatum)
			{
				currentDatum = NULL;
			}
			PGrnSequentialSearchDatumFinalize(datum);
			grn_hash_cursor_delete(ctx, cursor, NULL);
		}
	}
	GRN_HASH_EACH_END(ctx, cursor);
	GRN_LOG(ctx, GRN_LOG_DEBUG, "%s[end] %u", tag, grn_hash_size(ctx, data));
}

void
PGrnSequentialSearchSetTargetText(const char *target, unsigned int targetSize)
{
	grn_obj *text = &(buffers->text);
	GRN_TEXT_SET(ctx, text, target, targetSize);
	currentTargetType = PGRN_SEQUENTIAL_SEARCH_TARGET_TEXT;
}

void
PGrnSequentialSearchSetTargetTexts(ArrayType *targets, PGrnCondition *condition)
{
	grn_obj *texts = &(buffers->texts);
	ArrayIterator iterator;
	int i;
	int nTargets = 0;
	Datum datum;
	bool isNULL;

	GRN_BULK_REWIND(texts);
	iterator = array_create_iterator(targets, 0, NULL);
	if (condition->isTargets)
		nTargets = GRN_BULK_VSIZE(condition->isTargets) / sizeof(bool);
	for (i = 0; array_iterate(iterator, &datum, &isNULL); i++)
	{
		const char *target = NULL;
		unsigned int targetSize = 0;
		unsigned int weight = 0;
		grn_id domain = GRN_DB_TEXT;

		if (nTargets > i && !GRN_BOOL_VALUE_AT(condition->isTargets, i))
			continue;

		if (isNULL)
			continue;

		PGrnPGDatumExtractString(
			datum, ARR_ELEMTYPE(targets), &target, &targetSize);
		if (!target)
			continue;

		grn_vector_add_element(ctx, texts, target, targetSize, weight, domain);
	}
	array_free_iterator(iterator);

	currentTargetType = PGRN_SEQUENTIAL_SEARCH_TARGET_TEXTS;
}

static bool
PGrnSequentialSearchPrepareIndex(PGrnCondition *condition,
								 PGrnSequentialSearchType type)
{
	const char *tag = "[sequential-search][index]";
	const char *schemaName = NULL;
	size_t schemaNameSize = 0;
	const char *indexName = NULL;
	size_t indexNameSize = 0;
	const char *columnName = NULL;
	size_t columnNameSize = 0;
	Oid indexOID = InvalidOid;
	int attributeIndex = -1;
	grn_column_flags indexFlags = GRN_OBJ_COLUMN_INDEX;
	bool targetIsVector =
		(currentTargetType == PGRN_SEQUENTIAL_SEARCH_TARGET_TEXTS);
	PGrnSequentialSearchDatumKey key = {0};

	if (!PGrnPGTextIsEmpty(condition->schemaName))
	{
		schemaName = VARDATA_ANY(condition->schemaName);
		schemaNameSize = VARSIZE_ANY_EXHDR(condition->schemaName);
	}
	if (!PGrnPGTextIsEmpty(condition->indexName))
	{
		indexName = VARDATA_ANY(condition->indexName);
		indexNameSize = VARSIZE_ANY_EXHDR(condition->indexName);
	}
	if (!PGrnPGTextIsEmpty(condition->columnName))
	{
		columnName = VARDATA_ANY(condition->columnName);
		columnNameSize = VARSIZE_ANY_EXHDR(condition->columnName);
	}

	if (indexNameSize > 0)
	{
		grn_obj *text = &(buffers->general);

		grn_obj_reinit(ctx, text, GRN_DB_TEXT, 0);
		if (schemaNameSize > 0)
		{
			GRN_TEXT_SET(ctx, text, schemaName, schemaNameSize);
			GRN_TEXT_PUTC(ctx, text, '.');
		}
		GRN_TEXT_SET(ctx, text, indexName, indexNameSize);
		GRN_TEXT_PUTC(ctx, text, '\0');
		indexOID = PGrnPGIndexNameToID(GRN_TEXT_VALUE(text));
	}
	if (OidIsValid(indexOID) && columnNameSize > 0)
	{
		Relation index;
		index = PGrnPGResolveIndexID(indexOID);
		attributeIndex =
			PGrnPGResolveAttributeIndex(index, columnName, columnNameSize);
		RelationClose(index);
	}

	key.indexOID = indexOID;
	key.attributeIndex = attributeIndex;
	key.targetType = currentTargetType;
	key.useIndex = (PGrnIsTemporaryIndexSearchAvailable &&
					(OidIsValid(indexOID) || targetIsVector));
	key.type = type;
	if (currentDatum && currentDatum->indexOID == key.indexOID &&
		currentDatum->attributeIndex == key.attributeIndex &&
		currentDatum->targetType == key.targetType &&
		currentDatum->useIndex == key.useIndex &&
		currentDatum->type == key.type)
	{
		currentDatum->used = true;
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
	currentDatum->attributeIndex = key.attributeIndex;
	currentDatum->useIndex = key.useIndex;
	currentDatum->type = key.type;
	currentDatum->fuzzyMaxDistanceRatio = condition->fuzzyMaxDistanceRatio;
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
		if (!isPGroongaIndex)
		{
			PGrnSequentialSearchDatumFinalize(currentDatum);
			grn_hash_delete(ctx, data, &key, sizeof(key), NULL);
			currentDatum = NULL;
			RelationClose(index);
			PGrnCheckRC(GRN_INVALID_ARGUMENT,
						"%s[invalid] not PGroonga index: <%.*s%s%.*s%s%.*s>",
						tag,
						(int) schemaNameSize,
						schemaName,
						schemaNameSize > 0 ? "." : "",
						(int) indexNameSize,
						indexName,
						columnNameSize > 0 ? "." : "",
						(int) columnNameSize,
						columnName);
		}

		PG_TRY();
		{
			currentDatum->lexicon = PGrnCreateSimilarTemporaryLexicon(
				index, columnName, columnNameSize, tag);
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

		if (grn_obj_get_info(
				ctx, currentDatum->lexicon, GRN_INFO_DEFAULT_TOKENIZER, NULL))
		{
			indexFlags |= GRN_OBJ_WITH_POSITION;
		}

		PG_TRY();
		{
			currentDatum->indexColumn = PGrnCreateColumn(NULL,
														 currentDatum->lexicon,
														 "index",
														 indexFlags,
														 currentDatum->table);
			PGrnIndexColumnSetSource(
				NULL, currentDatum->indexColumn, currentDatum->targetColumn);
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
				PGrnCreateTable(NULL,
								NULL,
								tableFlags,
								grn_ctx_at(ctx, GRN_DB_SHORT_TEXT),
								tokenizer,
								normalizers,
								NULL);
			currentDatum->indexColumn = PGrnCreateColumn(NULL,
														 currentDatum->lexicon,
														 "index",
														 indexFlags,
														 currentDatum->table);
			PGrnIndexColumnSetSource(
				NULL, currentDatum->indexColumn, currentDatum->targetColumn);
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
PGrnSequentialSearchPrepareExpression(PGrnCondition *condition,
									  PGrnSequentialSearchType type)
{
	const char *tag = "[sequential-search][expression]";
	bool indexUpdated;
	XXH64_hash_t expressionHash;

	indexUpdated = PGrnSequentialSearchPrepareIndex(condition, type);
	expressionHash = XXH3_64bits(VARDATA_ANY(condition->query),
								 VARSIZE_ANY_EXHDR(condition->query));
	if (!indexUpdated && currentDatum->expressionHash == expressionHash)
		return true;

	if (currentDatum->expression)
	{
		grn_obj_close(ctx, currentDatum->expression);
		currentDatum->expression = NULL;
		currentDatum->expressionHash = 0;
	}

	GRN_EXPR_CREATE_FOR_QUERY(ctx,
							  currentDatum->table,
							  currentDatum->expression,
							  currentDatum->variable);
	if (!currentDatum->expression)
	{
		PGrnCheckRC(
			GRN_NO_MEMORY_AVAILABLE, "%s failed to create expression", tag);
	}

	currentDatum->expressionHash = expressionHash;

	return false;
}

void
PGrnSequentialSearchSetMatchTerm(PGrnCondition *condition)
{
	const char *tag = "[sequential-search][match-term]";

	if (PGrnSequentialSearchPrepareExpression(
			condition, PGRN_SEQUENTIAL_SEARCH_MATCH_TERM))
	{
		return;
	}

	PGrnExprAppendObject(currentDatum->expression,
						 currentDatum->targetColumn,
						 GRN_OP_GET_VALUE,
						 1,
						 tag,
						 NULL);
	PGrnExprAppendConstString(currentDatum->expression,
							  VARDATA_ANY(condition->query),
							  VARSIZE_ANY_EXHDR(condition->query),
							  GRN_OP_PUSH,
							  1,
							  tag);
	PGrnExprAppendOp(currentDatum->expression, GRN_OP_MATCH, 2, tag, NULL);
}

void
PGrnSequentialSearchSetEqualText(PGrnCondition *condition)
{
	const char *tag = "[sequential-search][equal-text]";

	if (PGrnSequentialSearchPrepareExpression(
			condition, PGRN_SEQUENTIAL_SEARCH_EQUAL_TEXT))
	{
		return;
	}

	PGrnExprAppendObject(currentDatum->expression,
						 currentDatum->targetColumn,
						 GRN_OP_GET_VALUE,
						 1,
						 tag,
						 NULL);
	PGrnExprAppendConstString(currentDatum->expression,
							  VARDATA_ANY(condition->query),
							  VARSIZE_ANY_EXHDR(condition->query),
							  GRN_OP_PUSH,
							  1,
							  tag);
	PGrnExprAppendOp(currentDatum->expression, GRN_OP_EQUAL, 2, tag, NULL);
}

void
PGrnSequentialSearchSetPrefix(PGrnCondition *condition)
{
	const char *tag = "[sequential-search][prefix]";

	if (PGrnSequentialSearchPrepareExpression(condition,
											  PGRN_SEQUENTIAL_SEARCH_PREFIX))
	{
		return;
	}

	PGrnExprAppendObject(currentDatum->expression,
						 currentDatum->targetColumn,
						 GRN_OP_GET_VALUE,
						 1,
						 tag,
						 NULL);
	PGrnExprAppendConstString(currentDatum->expression,
							  VARDATA_ANY(condition->query),
							  VARSIZE_ANY_EXHDR(condition->query),
							  GRN_OP_PUSH,
							  1,
							  tag);
	PGrnExprAppendOp(currentDatum->expression, GRN_OP_PREFIX, 2, tag, NULL);
}

void
PGrnSequentialSearchSetQuery(PGrnCondition *condition,
							 PGrnSequentialSearchType type)
{
	const char *tag = "[sequential-search][query]";
	const char *query = VARDATA_ANY(condition->query);
	size_t querySize = VARSIZE_ANY_EXHDR(condition->query);

	if (PGrnSequentialSearchPrepareExpression(condition, type))
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
	PGrnCheck(
		"%s failed to parse expression: <%.*s>", tag, (int) querySize, query);
}

void
PGrnSequentialSearchSetScript(PGrnCondition *condition)
{
	const char *tag = "[sequential-search][query]";
	const char *script = VARDATA_ANY(condition->query);
	size_t scriptSize = VARSIZE_ANY_EXHDR(condition->query);
	grn_expr_flags flags = GRN_EXPR_SYNTAX_SCRIPT;

	if (PGrnSequentialSearchPrepareExpression(condition,
											  PGRN_SEQUENTIAL_SEARCH_SCRIPT))
	{
		return;
	}

	grn_expr_parse(ctx,
				   currentDatum->expression,
				   script,
				   scriptSize,
				   currentDatum->targetColumn,
				   GRN_OP_MATCH,
				   GRN_OP_AND,
				   flags);
	if (ctx->rc != GRN_SUCCESS)
		currentDatum->expressionHash = 0;
	PGrnCheck(
		"%s failed to parse expression: <%.*s>", tag, (int) scriptSize, script);
}

void
PGrnSequentialSearchSetRegexp(PGrnCondition *condition)
{
	const char *tag = "[sequential-search][regexp]";

	if (PGrnSequentialSearchPrepareExpression(condition,
											  PGRN_SEQUENTIAL_SEARCH_REGEXP))
	{
		return;
	}

	PGrnExprAppendObject(currentDatum->expression,
						 currentDatum->targetColumn,
						 GRN_OP_GET_VALUE,
						 1,
						 tag,
						 NULL);
	PGrnExprAppendConstString(currentDatum->expression,
							  VARDATA_ANY(condition->query),
							  VARSIZE_ANY_EXHDR(condition->query),
							  GRN_OP_PUSH,
							  1,
							  tag);
	PGrnExprAppendOp(currentDatum->expression, GRN_OP_REGEXP, 2, tag, NULL);
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
		grn_table_selector *table_selector = grn_table_selector_open(
			ctx, currentDatum->table, currentDatum->expression, GRN_OP_OR);
		grn_table_selector_set_fuzzy_max_distance_ratio(
			ctx, table_selector, currentDatum->fuzzyMaxDistanceRatio);
		grn_table_selector_select(ctx, table_selector, currentDatum->matched);
		grn_table_selector_close(ctx, table_selector);

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
