#include "pgroonga.h"

#include "pgrn_global.h"
#include "pgrn_groonga.h"
#include "pgrn_highlight_html.h"

#include <catalog/pg_type.h>
#include <utils/array.h>
#include <utils/builtins.h>

static grn_ctx *ctx = &PGrnContext;
static grn_obj *PGrnKeywordsTable = NULL;
static grn_obj PGrnKeywordIDs;

void
PGrnInitializeHighlightHTML(void)
{
	PGrnKeywordsTable = grn_table_create(ctx, NULL, 0, NULL,
										 GRN_OBJ_TABLE_PAT_KEY,
										 grn_ctx_at(ctx, GRN_DB_SHORT_TEXT),
										 0);
	grn_obj_set_info(ctx,
					 PGrnKeywordsTable,
					 GRN_INFO_NORMALIZER,
					 grn_ctx_get(ctx, "NormalizerAuto", -1));

	GRN_RECORD_INIT(&PGrnKeywordIDs,
					GRN_OBJ_VECTOR,
					grn_obj_id(ctx, PGrnKeywordsTable));
}

void
PGrnFinalizeHighlightHTML(void)
{
	if (!PGrnKeywordsTable)
		return;

	GRN_OBJ_FIN(ctx, &PGrnKeywordIDs);

	grn_obj_close(ctx, PGrnKeywordsTable);
	PGrnKeywordsTable = NULL;
}

static void
PGrnKeywordsTableUpdate(ArrayType *keywords)
{
	{
		int i, n;

		GRN_BULK_REWIND(&PGrnKeywordIDs);

		n = ARR_DIMS(keywords)[0];
		for (i = 1; i <= n; i++)
		{
			Datum keywordDatum;
			text *keyword;
			bool isNULL;
			grn_id id;

			keywordDatum = array_ref(keywords, 1, &i, -1, -1, false,
									 'i', &isNULL);
			if (isNULL)
				continue;

			keyword = DatumGetTextPP(keywordDatum);
			id = grn_table_add(ctx, PGrnKeywordsTable,
							   VARDATA_ANY(keyword),
							   VARSIZE_ANY_EXHDR(keyword),
							   NULL);
			if (id == GRN_ID_NIL)
				continue;
			GRN_RECORD_PUT(ctx, &PGrnKeywordIDs, id);
		}
	}

	{
		grn_table_cursor *cursor;
		grn_id id;
		size_t nIDs;

		cursor = grn_table_cursor_open(ctx,
									   PGrnKeywordsTable,
									   NULL, 0,
									   NULL, 0,
									   0, -1, 0);
		if (!cursor) {
			ereport(ERROR,
					(errcode(ERRCODE_OUT_OF_MEMORY),
					 errmsg("pgroonga: "
							"failed to create cursor for PGrnKeywordsTable: %s",
							ctx->errbuf)));
		}

		nIDs = GRN_BULK_VSIZE(&PGrnKeywordIDs) / sizeof(grn_id);
		while ((id = grn_table_cursor_next(ctx, cursor)) != GRN_ID_NIL)
		{
			size_t i;
			bool specified = false;

			for (i = 0; i < nIDs; i++)
			{
				if (id == GRN_RECORD_VALUE_AT(&PGrnKeywordIDs, i))
				{
					specified = true;
					break;
				}
			}

			if (specified)
				continue;

			grn_table_cursor_delete(ctx, cursor);
		}

		grn_table_cursor_close(ctx, cursor);
	}
}

static text *
PGrnHighlightHTML(text *target)
{
	grn_obj buffer;
	text *highlighted;

	GRN_TEXT_INIT(&buffer, 0);

	{
		const char *openTag = "<span class=\"keyword\">";
		size_t openTagLength = strlen(openTag);
		const char *closeTag = "</span>";
		size_t closeTagLength = strlen(closeTag);
		const char *string;
		size_t stringLength;

		string = VARDATA_ANY(target);
		stringLength = VARSIZE_ANY_EXHDR(target);

		while (stringLength > 0) {
#define MAX_N_HITS 16
			grn_pat_scan_hit hits[MAX_N_HITS];
			const char *rest;
			int i, nHits;
			size_t previous = 0;
			size_t chunkLength;

			nHits = grn_pat_scan(ctx, (grn_pat *)PGrnKeywordsTable,
								 string, stringLength,
								 hits, MAX_N_HITS, &rest);
			for (i = 0; i < nHits; i++) {
				if ((hits[i].offset - previous) > 0) {
					grn_text_escape_xml(ctx,
										&buffer,
										string + previous,
										hits[i].offset - previous);
				}
				GRN_TEXT_PUT(ctx, &buffer, openTag, openTagLength);
				grn_text_escape_xml(ctx,
									&buffer,
									string + hits[i].offset,
									hits[i].length);
				GRN_TEXT_PUT(ctx, &buffer, closeTag, closeTagLength);
				previous = hits[i].offset + hits[i].length;
			}

			chunkLength = rest - string;
			if ((chunkLength - previous) > 0) {
				grn_text_escape_xml(ctx,
									&buffer,
									string + previous,
									stringLength - previous);
			}
			stringLength -= chunkLength;
			string = rest;
#undef MAX_N_HITS
		}
	}

	highlighted = cstring_to_text_with_len(GRN_TEXT_VALUE(&buffer),
										   GRN_TEXT_LEN(&buffer));
	GRN_OBJ_FIN(ctx, &buffer);
	return highlighted;
}

/**
 * pgroonga.highlight_html(target text, keywords text[]) : text
 */
Datum
pgroonga_highlight_html(PG_FUNCTION_ARGS)
{
	text *target = PG_GETARG_TEXT_PP(0);
	ArrayType *keywords = PG_GETARG_ARRAYTYPE_P(1);
	text *highlighted;

	PGrnKeywordsTableUpdate(keywords);
	highlighted = PGrnHighlightHTML(target);

	PG_RETURN_TEXT_P(highlighted);
}
