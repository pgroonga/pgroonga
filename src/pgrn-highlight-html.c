#include "pgroonga.h"

#include "pgrn-compatible.h"

#include "pgrn-global.h"
#include "pgrn-groonga.h"
#include "pgrn-highlight-html.h"
#include "pgrn-options.h"
#include "pgrn-pg.h"

#include <catalog/pg_type.h>
#include <utils/array.h>
#include <utils/builtins.h>

static grn_ctx *ctx = &PGrnContext;
static struct PGrnBuffers *buffers = &PGrnBuffers;
static grn_highlighter *highlighter = NULL;
static Oid indexOID = InvalidOid;
static grn_obj *lexicon = NULL;

PGRN_FUNCTION_INFO_V1(pgroonga_highlight_html);

void
PGrnInitializeHighlightHTML(void)
{
	highlighter = grn_highlighter_open(ctx);
}

void
PGrnFinalizeHighlightHTML(void)
{
	if (highlighter)
	{
		grn_highlighter_close(ctx, highlighter);
		highlighter = NULL;
	}

	indexOID = InvalidOid;

	if (lexicon)
	{
		grn_obj_close(ctx, lexicon);
		lexicon = NULL;
	}
}

static text *
PGrnHighlightHTML(text *target)
{
	grn_obj *buffer = &(buffers->general);
	text *highlighted;

	grn_obj_reinit(ctx, buffer, GRN_DB_TEXT, 0);
	grn_highlighter_highlight(ctx,
							  highlighter,
							  VARDATA_ANY(target),
							  VARSIZE_ANY_EXHDR(target),
							  buffer);
	highlighted = cstring_to_text_with_len(GRN_TEXT_VALUE(buffer),
										   GRN_TEXT_LEN(buffer));
	return highlighted;
}

static void
PGrnHighlightHTMLUpdateKeywords(ArrayType *keywords)
{
	int i, n;

	if (ARR_NDIM(keywords) == 0)
	{
		n = 0;
	}
	else
	{
		n = ARR_DIMS(keywords)[0];
	}

	for (i = 1; i <= n; i++)
	{
		Datum keywordDatum;
		text *keyword;
		bool isNULL;

		keywordDatum = array_ref(keywords, 1, &i, -1, -1, false, 'i', &isNULL);
		if (isNULL)
			continue;

		keyword = DatumGetTextPP(keywordDatum);
		grn_highlighter_add_keyword(ctx,
									highlighter,
									VARDATA_ANY(keyword),
									VARSIZE_ANY_EXHDR(keyword));
	}
}

static void
PGrnHighlightHTMLSetLexicon(const char *indexName)
{
	Oid oid;
	grn_obj *tokenizer = NULL;
	grn_obj *normalizer = NULL;
	grn_obj *tokenFilters = &(buffers->tokenFilters);
	grn_table_flags flags = 0;

	if (!indexName || indexName[0] == '\0')
	{
		indexOID = InvalidOid;
		grn_highlighter_set_lexicon(ctx, highlighter, NULL);
		return;
	}

	oid = PGrnPGIndexNameToID(indexName);
	if (indexOID == oid)
		return;

	if (!OidIsValid(oid))
	{
		indexOID = InvalidOid;
		grn_highlighter_set_lexicon(ctx, highlighter, NULL);
		return;
	}

	{
		Relation index = PGrnPGResolveIndexName(indexName);
		GRN_BULK_REWIND(tokenFilters);
		PGrnApplyOptionValues(index,
							  PGRN_OPTION_USE_CASE_FULL_TEXT_SEARCH,
							  &tokenizer, PGRN_DEFAULT_TOKENIZER,
							  &normalizer, PGRN_DEFAULT_NORMALIZER,
							  tokenFilters,
							  &flags);
		RelationClose(index);
	}

	if (lexicon)
		grn_obj_close(ctx, lexicon);
	lexicon = PGrnCreateTable(InvalidRelation,
							  NULL,
							  flags,
							  grn_ctx_at(ctx, GRN_DB_SHORT_TEXT),
							  tokenizer,
							  normalizer,
							  tokenFilters);
	grn_highlighter_set_lexicon(ctx, highlighter, lexicon);
	PGrnCheck("highlight-html: failed to set lexicon");
	indexOID = oid;
}

/**
 * pgroonga.highlight_html(target text, keywords text[]) : text
 * pgroonga.highlight_html(target text, keywords text[], indexName cstring) : text
 */
Datum
pgroonga_highlight_html(PG_FUNCTION_ARGS)
{
	text *target = PG_GETARG_TEXT_PP(0);
	ArrayType *keywords = PG_GETARG_ARRAYTYPE_P(1);
	text *highlighted;

	PGrnHighlightHTMLUpdateKeywords(keywords);

	if (PG_NARGS() == 3)
	{
		const char *indexName = PG_GETARG_CSTRING(2);
		PGrnHighlightHTMLSetLexicon(indexName);
	}
	else
	{
		PGrnHighlightHTMLSetLexicon(NULL);
	}

	highlighted = PGrnHighlightHTML(target);

	PG_RETURN_TEXT_P(highlighted);
}
