#include "pgroonga.h"

#include "pgrn-groonga.h"
#include "pgrn-jsonb.h"
#include "pgrn-pg.h"

#include <utils/builtins.h>

#include <string.h>

static grn_obj json;

PGDLLEXPORT PG_FUNCTION_INFO_V1(pgroonga_explain_match);

void
PGrnInitializeExplainMatch(void)
{
	GRN_TEXT_INIT(&json, 0);
}

void
PGrnFinalizeExplainMatch(void)
{
	GRN_OBJ_FIN(ctx, &json);
}

static int
PGrnExplainMatchResolveLexiconIndex(Relation index,
									const char *attributeName,
									size_t attributeNameSize)
{
	int i;

	if (attributeNameSize > 0)
	{
		i = PGrnPGResolveAttributeIndex(index, attributeName, attributeNameSize);
		if (i != -1)
			return i;
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("pgroonga: [explain-match] "
						"index doesn't have the specified attribute: <%s.%.*s>",
						RelationGetRelationName(index),
						(int) attributeNameSize,
						attributeName)));
	}

	for (i = 0; i < index->rd_att->natts; i++)
	{
		grn_obj *lexicon;
		bool isTextLexicon;

		lexicon = PGrnLookupLexicon(index, i, ERROR);
		isTextLexicon = grn_type_id_is_text_family(ctx, lexicon->header.domain);
		grn_obj_unref(ctx, lexicon);
		if (isTextLexicon)
			return i;
	}

	ereport(ERROR,
			(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			 errmsg("pgroonga: [explain-match] "
					"index doesn't have a lexicon for text: <%s>",
					RelationGetRelationName(index))));
	return -1;
}

static void
PGrnExplainMatchOutputText(grn_content_type type,
						   const char *name,
						   const char *value,
						   size_t valueSize)
{
	grn_output_cstr(ctx, &json, type, name);
	grn_output_str(ctx, &json, type, value, valueSize);
}

static void
PGrnExplainMatchOutputLexiconName(grn_content_type type, grn_obj *lexicon)
{
	char lexiconName[GRN_TABLE_MAX_KEY_SIZE];
	int lexiconNameSize;

	lexiconNameSize =
		grn_obj_name(ctx, lexicon, lexiconName, sizeof(lexiconName));
	PGrnExplainMatchOutputText(
		type, "lexicon_name", lexiconName, lexiconNameSize);
}

/**
 * pgroonga_explain_match(indexName cstring, query text) : jsonb
 */
Datum
pgroonga_explain_match(PG_FUNCTION_ARGS)
{
	char *fullIndexName = PG_GETARG_CSTRING(0);
	text *query = PG_GETARG_TEXT_PP(1);
	const char *indexName;
	size_t indexNameSize;
	const char *attributeName;
	size_t attributeNameSize;
	char *nulTerminatedIndexName;
	Relation index;
	int lexiconIndex;
	grn_obj *lexicon;
	grn_obj *temporaryLexicon;
	grn_token_cursor *tokenCursor;
	grn_obj matchedTerms;
	grn_content_type type = GRN_CONTENT_JSON;
	Jsonb *jsonb;

	PGrnPGFullIndexNameSplit(fullIndexName,
							 strlen(fullIndexName),
							 &indexName,
							 &indexNameSize,
							 &attributeName,
							 &attributeNameSize);
	nulTerminatedIndexName = pnstrdup(indexName, indexNameSize);
	index = PGrnPGResolveIndexName(nulTerminatedIndexName);

	lexiconIndex = PGrnExplainMatchResolveLexiconIndex(index,
													   attributeName,
													   attributeNameSize);
	lexicon = PGrnLookupLexicon(index, lexiconIndex, ERROR);
	temporaryLexicon = PGrnCreateSimilarTemporaryLexicon(index,
														 attributeName,
														 attributeNameSize,
														 "[explain-match]");

	GRN_BULK_REWIND(&json);
	grn_output_map_open(ctx, &json, type, "explain_match", 6);
	PGrnExplainMatchOutputText(
		type, "query", VARDATA_ANY(query), VARSIZE_ANY_EXHDR(query));
	PGrnExplainMatchOutputText(type, "index_name", indexName, indexNameSize);
	if (attributeNameSize > 0)
	{
		PGrnExplainMatchOutputText(
			type, "attribute_name", attributeName, attributeNameSize);
	}
	else
	{
		grn_output_cstr(ctx, &json, type, "attribute_name");
		grn_output_null(ctx, &json, type);
	}
	PGrnExplainMatchOutputLexiconName(type, lexicon);

	GRN_TEXT_INIT(&matchedTerms, GRN_OBJ_VECTOR);

	grn_output_cstr(ctx, &json, type, "tokens");
	grn_output_array_open(ctx, &json, type, "tokens", -1);

	tokenCursor = grn_token_cursor_open(ctx,
										temporaryLexicon,
										VARDATA_ANY(query),
										VARSIZE_ANY_EXHDR(query),
										GRN_TOKEN_ADD,
										0);
	PGrnCheck("[explain-match] failed to create token cursor");

	while (grn_token_cursor_get_status(ctx, tokenCursor) ==
		   GRN_TOKEN_CURSOR_DOING)
	{
		grn_id id = grn_token_cursor_next(ctx, tokenCursor);
		grn_token *token;
		grn_obj *data;
		grn_id lexiconID;

		if (id == GRN_ID_NIL)
			continue;

		token = grn_token_cursor_get_token(ctx, tokenCursor);
		data = grn_token_get_data(ctx, token);
		lexiconID = grn_table_get(
			ctx, lexicon, GRN_TEXT_VALUE(data), GRN_TEXT_LEN(data));

		grn_output_map_open(ctx, &json, type, "token", 6);
		PGrnExplainMatchOutputText(
			type, "value", GRN_TEXT_VALUE(data), GRN_TEXT_LEN(data));
		grn_output_cstr(ctx, &json, type, "position");
		grn_output_uint32(ctx, &json, type, grn_token_get_position(ctx, token));
		grn_output_cstr(ctx, &json, type, "source_offset");
		grn_output_uint64(
			ctx, &json, type, grn_token_get_source_offset(ctx, token));
		grn_output_cstr(ctx, &json, type, "source_length");
		grn_output_uint32(
			ctx, &json, type, grn_token_get_source_length(ctx, token));
		grn_output_cstr(ctx, &json, type, "source_first_character_length");
		grn_output_uint32(ctx,
						  &json,
						  type,
						  grn_token_get_source_first_character_length(ctx,
																	   token));
		grn_output_cstr(ctx, &json, type, "in_lexicon");
		grn_output_bool(ctx, &json, type, lexiconID != GRN_ID_NIL);
		grn_output_map_close(ctx, &json, type);

		if (lexiconID != GRN_ID_NIL)
		{
			grn_vector_add_element(ctx,
								   &matchedTerms,
								   GRN_TEXT_VALUE(data),
								   GRN_TEXT_LEN(data),
								   0,
								   GRN_DB_TEXT);
		}
	}
	grn_token_cursor_close(ctx, tokenCursor);

	grn_output_array_close(ctx, &json, type);
	grn_output_cstr(ctx, &json, type, "matched_terms");
	{
		unsigned int i;
		unsigned int nMatchedTerms = grn_vector_size(ctx, &matchedTerms);

		grn_output_array_open(
			ctx, &json, type, "matched_terms", nMatchedTerms);
		for (i = 0; i < nMatchedTerms; i++)
		{
			const char *term;
			unsigned int termSize;

			termSize = grn_vector_get_element(
				ctx, &matchedTerms, i, &term, NULL, NULL);
			grn_output_str(ctx, &json, type, term, termSize);
		}
	}
	grn_output_array_close(ctx, &json, type);
	grn_output_map_close(ctx, &json, type);
	GRN_TEXT_PUTC(ctx, &json, '\0');

	jsonb = PGrnJSONBParse(GRN_TEXT_VALUE(&json));

	GRN_OBJ_FIN(ctx, &matchedTerms);
	grn_obj_close(ctx, temporaryLexicon);
	grn_obj_unref(ctx, lexicon);
	RelationClose(index);
	pfree(nulTerminatedIndexName);

	PG_RETURN_JSONB_P(jsonb);
}
