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

#include <xxhash.h>

static struct PGrnBuffers *buffers = &PGrnBuffers;
static grn_highlighter *highlighter = NULL;
static Oid indexOID = InvalidOid;
static grn_obj *lexicon = NULL;
static XXH3_state_t *hashState = NULL;
static XXH64_hash_t keywordsHash = 0;
static const char *keywordsHashDelimiter = "\0";
static const size_t keywordsHashDelimiterSize = 1;

/* For backward compatibility */
PGDLLEXPORT PG_FUNCTION_INFO_V1(pgroonga_highlight_html);
PGDLLEXPORT PG_FUNCTION_INFO_V1(pgroonga_highlight_html_text);
PGDLLEXPORT PG_FUNCTION_INFO_V1(pgroonga_highlight_html_text_array);

void
PGrnInitializeHighlightHTML(void)
{
	highlighter = grn_highlighter_open(ctx);
	hashState = XXH3_createState();
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

	if (hashState)
	{
		XXH3_freeState(hashState);
		hashState = NULL;
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
	highlighted =
		cstring_to_text_with_len(GRN_TEXT_VALUE(buffer), GRN_TEXT_LEN(buffer));
	return highlighted;
}

static void
PGrnHighlightHTMLClearKeywords(void)
{
	grn_highlighter_clear_keywords(ctx, highlighter);
}

static void
PGrnHighlightHTMLUpdateKeywords(ArrayType *keywords)
{
	if (ARR_NDIM(keywords) != 1)
	{
		if (keywordsHash != 0)
		{
			PGrnHighlightHTMLClearKeywords();
			keywordsHash = 0;
		}
		return;
	}

	if (keywordsHash == 0)
	{
		ArrayIterator iterator;
		Datum datum;
		bool isNULL;

		PGrnHighlightHTMLClearKeywords();
		XXH3_64bits_reset(hashState);
		iterator = array_create_iterator(keywords, 0, NULL);
		while (array_iterate(iterator, &datum, &isNULL))
		{
			text *keyword;

			if (isNULL)
				continue;

			keyword = DatumGetTextPP(datum);
			grn_highlighter_add_keyword(ctx,
										highlighter,
										VARDATA_ANY(keyword),
										VARSIZE_ANY_EXHDR(keyword));
			XXH3_64bits_update(
				hashState, VARDATA_ANY(keyword), VARSIZE_ANY_EXHDR(keyword));
			XXH3_64bits_update(
				hashState, keywordsHashDelimiter, keywordsHashDelimiterSize);
		}
		array_free_iterator(iterator);
		keywordsHash = XXH3_64bits_digest(hashState);
		return;
	}

	{
		ArrayIterator iterator;
		Datum datum;
		bool isNULL;
		XXH64_hash_t newKeywordsHash;

		XXH3_64bits_reset(hashState);
		iterator = array_create_iterator(keywords, 0, NULL);
		while (array_iterate(iterator, &datum, &isNULL))
		{
			text *keyword;

			if (isNULL)
				continue;

			keyword = DatumGetTextPP(datum);
			XXH3_64bits_update(
				hashState, VARDATA_ANY(keyword), VARSIZE_ANY_EXHDR(keyword));
			XXH3_64bits_update(
				hashState, keywordsHashDelimiter, keywordsHashDelimiterSize);
		}
		array_free_iterator(iterator);
		newKeywordsHash = XXH3_64bits_digest(hashState);
		if (keywordsHash == newKeywordsHash)
			return;

		keywordsHash = newKeywordsHash;
	}

	{
		ArrayIterator iterator;
		Datum datum;
		bool isNULL;

		PGrnHighlightHTMLClearKeywords();
		iterator = array_create_iterator(keywords, 0, NULL);
		while (array_iterate(iterator, &datum, &isNULL))
		{
			text *keyword;

			if (isNULL)
				continue;

			keyword = DatumGetTextPP(datum);
			grn_highlighter_add_keyword(ctx,
										highlighter,
										VARDATA_ANY(keyword),
										VARSIZE_ANY_EXHDR(keyword));
		}
		array_free_iterator(iterator);
	}
}

static void
PGrnHighlightHTMLSetLexicon(const char *fullIndexName)
{
	const char *tag = "[highlight-html]";
	grn_obj *buffer = &(buffers->general);
	const char *indexName;
	const char *indexNameData = NULL;
	size_t indexNameSize = 0;
	const char *attributeNameData = NULL;
	size_t attributeNameSize = 0;
	Oid oid;
	Relation index;

	if (fullIndexName)
	{
		PGrnPGFullIndexNameSplit(fullIndexName,
								 strlen(fullIndexName),
								 &indexNameData,
								 &indexNameSize,
								 &attributeNameData,
								 &attributeNameSize);
	}
	if (indexNameSize == 0)
	{
		indexOID = InvalidOid;
		grn_highlighter_set_lexicon(ctx, highlighter, NULL);
		return;
	}

	grn_obj_reinit(ctx, buffer, GRN_DB_TEXT, 0);
	GRN_TEXT_SET(ctx, buffer, indexNameData, indexNameSize);
	GRN_TEXT_PUTC(ctx, buffer, '\0');
	indexName = GRN_TEXT_VALUE(buffer);
	oid = PGrnPGIndexNameToID(indexName);
	if (indexOID == oid)
		return;

	if (!OidIsValid(oid))
	{
		indexOID = InvalidOid;
		grn_highlighter_set_lexicon(ctx, highlighter, NULL);
		return;
	}

	index = PGrnPGResolveIndexName(indexName);
	if (lexicon)
	{
		grn_highlighter_set_lexicon(ctx, highlighter, NULL);
		grn_obj_close(ctx, lexicon);
		lexicon = NULL;
	}
	PG_TRY();
	{
		lexicon = PGrnCreateSimilarTemporaryLexicon(
			index, attributeNameData, attributeNameSize, tag);
	}
	PG_CATCH();
	{
		RelationClose(index);
		PG_RE_THROW();
	}
	PG_END_TRY();
	RelationClose(index);

	grn_highlighter_set_lexicon(ctx, highlighter, lexicon);
	PGrnCheck("%s failed to set lexicon", tag);
	indexOID = oid;
}

/* For backward compatibility. */
Datum
pgroonga_highlight_html(PG_FUNCTION_ARGS)
{
	return pgroonga_highlight_html_text(fcinfo);
}

/**
 * pgroonga.highlight_html(target text, keywords text[]) : text
 * pgroonga.highlight_html(target text, keywords text[], indexName cstring) :
 * text
 */
Datum
pgroonga_highlight_html_text(PG_FUNCTION_ARGS)
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

/**
 * pgroonga.highlight_html(target text[], keywords text[]) : text[]
 * pgroonga.highlight_html(target text[], keywords text[], indexName cstring) :
 * text[]
 */
Datum
pgroonga_highlight_html_text_array(PG_FUNCTION_ARGS)
{
	ArrayType *targets = PG_GETARG_ARRAYTYPE_P(0);
	ArrayType *keywords = PG_GETARG_ARRAYTYPE_P(1);
	int n;
	Datum *highlights;
	bool *nulls;

	n = ARR_DIMS(targets)[0];

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

	{
		int i = 0;
		ArrayIterator iterator;
		Datum datum;
		bool isNULL;

		highlights = palloc(sizeof(Datum) * n);
		nulls = palloc(sizeof(bool) * n);
		iterator = array_create_iterator(targets, 0, NULL);
		while (array_iterate(iterator, &datum, &isNULL))
		{
			nulls[i] = isNULL;
			if (isNULL)
			{
				highlights[i] = (Datum) 0;
			}
			else
			{
				text *target;
				text *highlighted;

				target = DatumGetTextPP(datum);
				highlighted = PGrnHighlightHTML(target);
				highlights[i] = PointerGetDatum(highlighted);
			}
			i++;
		}
	}

	{
		int dims[1];
		int lbs[1];

		dims[0] = n;
		lbs[0] = 1;
		PG_RETURN_POINTER(construct_md_array(
			highlights, nulls, 1, dims, lbs, TEXTOID, -1, false, 'i'));
	}
}
