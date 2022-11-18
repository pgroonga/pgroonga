#include "pgroonga.h"

#include "pgrn-compatible.h"

#include "pgrn-global.h"
#include "pgrn-groonga.h"

#include <catalog/pg_type.h>
#include <utils/array.h>
#include <utils/builtins.h>

static grn_ctx *ctx = &PGrnContext;

PGDLLEXPORT PG_FUNCTION_INFO_V1(pgroonga_snippet_html);

static grn_obj *
PGrnSnipCreate(ArrayType *keywords, const char *tag, unsigned int width)
{
	grn_obj *snip;
	int flags = GRN_SNIP_SKIP_LEADING_SPACES;
	unsigned int maxNResults = 3;
	const char *openTag = "<span class=\"keyword\">";
	const char *closeTag = "</span>";
	grn_snip_mapping *mapping = GRN_SNIP_MAPPING_HTML_ESCAPE;

	snip = grn_snip_open(ctx, flags, width, maxNResults,
						 openTag, strlen(openTag),
						 closeTag, strlen(closeTag),
						 mapping);
	if (!snip)
	{
		PGrnCheckRC(GRN_NO_MEMORY_AVAILABLE,
					"%s failed to allocate memory for generating snippet",
					tag);
		return NULL;
	}

	grn_snip_set_normalizer(ctx, snip, GRN_NORMALIZER_AUTO);

	{
		int i, n;

		if (ARR_NDIM(keywords) == 0)
			n = 0;
		else
			n = ARR_DIMS(keywords)[0];
		for (i = 1; i <= n; i++)
		{
			Datum keywordDatum;
			text *keyword;
			bool isNULL;

			keywordDatum = array_ref(keywords, 1, &i, -1, -1, false,
									 'i', &isNULL);
			if (isNULL)
				continue;

			keyword = DatumGetTextPP(keywordDatum);
			grn_snip_add_cond(ctx, snip,
							  VARDATA_ANY(keyword),
							  VARSIZE_ANY_EXHDR(keyword),
							  NULL, 0, NULL, 0);
		}
	}

	return snip;
}

static grn_rc
PGrnSnipExec(grn_obj *snip, text *target, ArrayType **snippetArray)
{
	grn_rc rc;
	unsigned int i, nResults, maxTaggedLength;
	char *buffer;
	Datum *snippets;
	int	dims[1];
	int	lbs[1];

	rc = grn_snip_exec(ctx, snip,
					   VARDATA_ANY(target),
					   VARSIZE_ANY_EXHDR(target),
					   &nResults, &maxTaggedLength);
	if (rc != GRN_SUCCESS)
	{
		return rc;
	}

	if (nResults == 0)
	{
		*snippetArray = construct_empty_array(TEXTOID);
		return GRN_SUCCESS;
	}

	buffer = palloc(sizeof(char) * maxTaggedLength);
	snippets = palloc(sizeof(Datum) * nResults);
	for (i = 0; i < nResults; i++)
	{
		grn_rc rc;
		unsigned int snippetLength = 0;

		rc = grn_snip_get_result(ctx, snip, i, buffer, &snippetLength);
		if (rc != GRN_SUCCESS)
		{
			pfree(buffer);
			return rc;
		}
		snippets[i] = PointerGetDatum(cstring_to_text_with_len(buffer,
															   snippetLength));
	}
	pfree(buffer);

	dims[0] = nResults;
	lbs[0] = 1;

	*snippetArray = construct_md_array(snippets, NULL,
									   1, dims, lbs,
									   TEXTOID, -1, false, 'i');
	return GRN_SUCCESS;
}

/**
 * pgroonga_snippet_html(target text, keywords text[], width integer DEFAULT 200) : text[]
 */
Datum
pgroonga_snippet_html(PG_FUNCTION_ARGS)
{
	const char *tag = "[snippet-html]";
	text *target = PG_GETARG_TEXT_PP(0);
	ArrayType *keywords = PG_GETARG_ARRAYTYPE_P(1);
	int width = PG_GETARG_INT32(2);
	grn_obj *snip;
	ArrayType *snippets;

	if (width <= 0)
	{
		ereport(ERROR,
		        (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
		         errmsg("%s width must be a positive number: %d",
		                tag,
		                width)));
	}

	snip = PGrnSnipCreate(keywords, tag, width);
	PGrnSnipExec(snip, target, &snippets);
	PG_TRY();
	{
		PGrnCheck("%s failed to compute snippets", tag);
	}
	PG_CATCH();
	{
		grn_obj_close(ctx, snip);
		PG_RE_THROW();
	}
	PG_END_TRY();
	grn_obj_close(ctx, snip);

	PG_RETURN_POINTER(snippets);
}
