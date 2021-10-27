#include "pgroonga.h"

#include "pgrn-compatible.h"

#include "pgrn-global.h"
#include "pgrn-groonga.h"
#include "pgrn-query-extract-keywords.h"

#include <catalog/pg_type.h>
#include <utils/array.h>
#include <utils/builtins.h>

static grn_ctx *ctx = &PGrnContext;
static grn_obj *table = NULL;
static grn_obj *textColumn = NULL;

PGDLLEXPORT PG_FUNCTION_INFO_V1(pgroonga_query_extract_keywords);

void
PGrnInitializeQueryExtractKeywords(void)
{
	table = grn_table_create(ctx, NULL, 0, NULL,
							 GRN_OBJ_TABLE_NO_KEY,
							 NULL,
							 NULL);
	textColumn = grn_column_create(ctx,
								   table,
								   "text", strlen("text"),
								   NULL,
								   GRN_OBJ_COLUMN_SCALAR,
								   grn_ctx_at(ctx, GRN_DB_TEXT));
}

void
PGrnFinalizeQueryExtractKeywords(void)
{
	if (textColumn)
	{
		grn_obj_close(ctx, textColumn);
		textColumn = NULL;
	}

	if (table)
	{
		grn_obj_close(ctx, table);
		table = NULL;
	}
}

static ArrayType *
PGrnQueryExtractKeywords(text *query)
{
	const char *tag = "[query-extract-keywords]";
	grn_obj *expression;
	grn_obj *variable;
	grn_expr_flags flags = PGRN_EXPR_QUERY_PARSE_FLAGS;
	ArrayType *keywords;

	GRN_EXPR_CREATE_FOR_QUERY(ctx, table, expression, variable);
	if (!expression)
	{
		PGrnCheckRC(GRN_NO_MEMORY_AVAILABLE,
					"%s failed to create expression",
					tag);
	}

	grn_expr_parse(ctx,
				   expression,
				   VARDATA_ANY(query),
				   VARSIZE_ANY_EXHDR(query),
				   textColumn,
				   GRN_OP_MATCH,
				   GRN_OP_AND,
				   flags);
	PGrnCheck("%s failed to parse expression: <%.*s>",
			  tag,
			  (int) VARSIZE_ANY_EXHDR(query),
			  VARDATA_ANY(query));

	{
		size_t i, nKeywords;
		grn_obj extractedKeywords;
		Datum *elements;
		int dims[1];
		int lbs[1];

		GRN_PTR_INIT(&extractedKeywords, GRN_OBJ_VECTOR, GRN_ID_NIL);
		grn_expr_get_keywords(ctx, expression, &extractedKeywords);
		nKeywords = GRN_BULK_VSIZE(&extractedKeywords) / sizeof(grn_obj *);
		elements = palloc(sizeof(Datum) * nKeywords);
		for (i = 0; i < nKeywords; i++) {
			grn_obj *extractedKeyword;
			text *keyword;

			extractedKeyword = GRN_PTR_VALUE_AT(&extractedKeywords, i);
			keyword = cstring_to_text_with_len(GRN_TEXT_VALUE(extractedKeyword),
											   GRN_TEXT_LEN(extractedKeyword));
			elements[i] = PointerGetDatum(keyword);
		}
		dims[0] = nKeywords;
		lbs[0] = 1;
		keywords = construct_md_array(elements,
									  NULL,
									  1,
									  dims,
									  lbs,
									  TEXTOID,
									  -1,
									  false,
									  'i');

		GRN_OBJ_FIN(ctx, &extractedKeywords);
	}

	return keywords;
}

/**
 * pgroonga.query_extract_keywords(query text) : text[]
 */
Datum
pgroonga_query_extract_keywords(PG_FUNCTION_ARGS)
{
	text *query = PG_GETARG_TEXT_PP(0);
	ArrayType *keywords;

	keywords = PGrnQueryExtractKeywords(query);

	PG_RETURN_POINTER(keywords);
}
