#include "pgroonga.h"

#include "pgrn_compatible.h"

#include "pgrn_global.h"
#include "pgrn_groonga.h"
#include "pgrn_highlight_html.h"
#include "pgrn_keywords.h"

#include <catalog/pg_type.h>
#include <utils/builtins.h>

static grn_ctx *ctx = &PGrnContext;
static grn_obj *keywordsTable = NULL;

PGRN_FUNCTION_INFO_V1(pgroonga_highlight_html);

void
PGrnInitializeHighlightHTML(void)
{
	keywordsTable = grn_table_create(ctx, NULL, 0, NULL,
									 GRN_OBJ_TABLE_PAT_KEY,
									 grn_ctx_at(ctx, GRN_DB_SHORT_TEXT),
									 NULL);
	grn_obj_set_info(ctx,
					 keywordsTable,
					 GRN_INFO_NORMALIZER,
					 grn_ctx_get(ctx, "NormalizerAuto", -1));
}

void
PGrnFinalizeHighlightHTML(void)
{
	if (!keywordsTable)
		return;

	grn_obj_close(ctx, keywordsTable);
	keywordsTable = NULL;
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

			nHits = grn_pat_scan(ctx, (grn_pat *)keywordsTable,
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

	PGrnKeywordsUpdateTable(keywords, keywordsTable);
	highlighted = PGrnHighlightHTML(target);

	PG_RETURN_TEXT_P(highlighted);
}
