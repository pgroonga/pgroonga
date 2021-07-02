#include "pgroonga.h"

#include "pgrn-global.h"
#include "pgrn-groonga.h"
#include "pgrn-keywords.h"
#include "pgrn-options.h"
#include "pgrn-pg.h"

#include <catalog/pg_type.h>
#include <utils/builtins.h>

static grn_ctx *ctx = &PGrnContext;
static struct PGrnBuffers *buffers = &PGrnBuffers;
static grn_obj keywordIDs;

void
PGrnInitializeKeywords(void)
{
	GRN_RECORD_INIT(&keywordIDs, GRN_OBJ_VECTOR, GRN_ID_NIL);
}

void
PGrnFinalizeKeywords(void)
{
	GRN_OBJ_FIN(ctx, &keywordIDs);
}

static bool
PGrnKeywordsResolveNormalizer(const char *indexName,
							  grn_obj **normalizers,
							  Oid *previousIndexID)
{
	if (!indexName || indexName[0] == '\0')
	{
		if (!previousIndexID)
			return true;
		return *previousIndexID != InvalidOid;
	}

	if (previousIndexID)
	{
		Oid indexID = PGrnPGIndexNameToID(indexName);
		if (indexID == *previousIndexID)
			return false;
		*previousIndexID = indexID;
	}

	{
		grn_obj *tokenizer = NULL;
		grn_obj *tokenFilters = NULL;
		grn_table_flags flags = 0;
		Relation index = PGrnPGResolveIndexName(indexName);
		PGrnApplyOptionValues(index,
							  -1,
							  PGRN_OPTION_USE_CASE_FULL_TEXT_SEARCH,
							  &tokenizer, PGRN_DEFAULT_TOKENIZER,
							  normalizers, PGRN_DEFAULT_NORMALIZERS,
							  &tokenFilters,
							  &flags);
		RelationClose(index);
	}
	return true;
}

void
PGrnKeywordsSetNormalizer(grn_obj *keywordsTable,
						  const char *indexName,
						  Oid *previousIndexID)
{
	grn_obj *normalizers = NULL;
	if (!PGrnKeywordsResolveNormalizer(indexName, &normalizers, previousIndexID))
		return;

	if (grn_table_size(ctx, keywordsTable) > 0)
		grn_table_truncate(ctx, keywordsTable);

	if (!normalizers)
	{
		normalizers = &(buffers->normalizers);
		GRN_TEXT_SETS(ctx, normalizers, PGRN_DEFAULT_NORMALIZERS);
	}
	grn_obj_set_info(ctx,
					 keywordsTable,
					 GRN_INFO_NORMALIZERS,
					 normalizers);
}

void
PGrnKeywordsUpdateTable(ArrayType *keywords, grn_obj *keywordsTable)
{
	{
		int i, n;

		GRN_BULK_REWIND(&keywordIDs);

		if (ARR_NDIM(keywords) == 0)
			n = 0;
		else
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
			id = grn_table_add(ctx, keywordsTable,
							   VARDATA_ANY(keyword),
							   VARSIZE_ANY_EXHDR(keyword),
							   NULL);
			if (id == GRN_ID_NIL)
				continue;
			GRN_RECORD_PUT(ctx, &keywordIDs, id);
		}
	}

	{
		grn_table_cursor *cursor;
		grn_id id;
		size_t nIDs;

		cursor = grn_table_cursor_open(ctx,
									   keywordsTable,
									   NULL, 0,
									   NULL, 0,
									   0, -1, 0);
		if (!cursor) {
			ereport(ERROR,
					(errcode(ERRCODE_OUT_OF_MEMORY),
					 errmsg("pgroonga: "
							"failed to create cursor for keywordsTable: %s",
							ctx->errbuf)));
		}

		nIDs = GRN_BULK_VSIZE(&keywordIDs) / sizeof(grn_id);
		while ((id = grn_table_cursor_next(ctx, cursor)) != GRN_ID_NIL)
		{
			size_t i;
			bool specified = false;

			for (i = 0; i < nIDs; i++)
			{
				if (id == GRN_RECORD_VALUE_AT(&keywordIDs, i))
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


