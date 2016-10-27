#include "pgroonga.h"

#include "pgrn_column_name.h"
#include "pgrn_create.h"
#include "pgrn_global.h"
#include "pgrn_groonga.h"
#include "pgrn_options.h"
#include "pgrn_value.h"
#include "pgrn_wal.h"

static grn_ctx *ctx = &PGrnContext;

void
PGrnCreateSourcesCtidColumn(PGrnCreateData *data)
{
	grn_column_flags flags = GRN_OBJ_COLUMN_SCALAR;
	grn_obj *type;

	type = grn_ctx_at(ctx, GRN_DB_UINT64);
	data->sourcesCtidColumn = PGrnCreateColumn(data->sourcesTable,
											   PGrnSourcesCtidColumnName,
											   GRN_OBJ_COLUMN_SCALAR,
											   type);
	PGrnWALCreateColumn(data->index,
						data->sourcesTable,
						PGrnSourcesCtidColumnName,
						flags,
						type);
}

void
PGrnCreateSourcesTable(PGrnCreateData *data)
{
	char sourcesTableName[GRN_TABLE_MAX_KEY_SIZE];
	grn_table_flags flags = GRN_OBJ_TABLE_NO_KEY;

	snprintf(sourcesTableName, sizeof(sourcesTableName),
			 PGrnSourcesTableNameFormat, data->relNode);
	data->sourcesTable = PGrnCreateTable(sourcesTableName,
										 GRN_OBJ_TABLE_NO_KEY,
										 NULL);
	PGrnWALCreateTable(data->index,
					   sourcesTableName,
					   flags,
					   NULL,
					   NULL,
					   NULL);

	PGrnCreateSourcesCtidColumn(data);
}

void
PGrnCreateDataColumn(PGrnCreateData *data)
{
	grn_column_flags flags = 0;
	grn_obj *range;
	grn_id rangeID;

	if (data->forPrefixSearch) {
		char lexiconName[GRN_TABLE_MAX_KEY_SIZE];

		snprintf(lexiconName, sizeof(lexiconName),
				 PGrnLexiconNameFormat, data->relNode, data->i);
		range = PGrnLookup(lexiconName, ERROR);
		rangeID = grn_obj_id(ctx, range);
	} else {
		rangeID = data->attributeTypeID;
		range = grn_ctx_at(ctx, rangeID);
	}

	if (data->attributeFlags & GRN_OBJ_VECTOR)
	{
		flags |= GRN_OBJ_COLUMN_VECTOR;
	}
	else
	{
		flags |= GRN_OBJ_COLUMN_SCALAR;

		if (PGrnIsLZ4Available)
		{
			switch (rangeID)
			{
			case GRN_DB_SHORT_TEXT:
			case GRN_DB_TEXT:
			case GRN_DB_LONG_TEXT:
				flags |= GRN_OBJ_COMPRESS_LZ4;
				break;
			}
		}
	}

	{
		char columnName[GRN_TABLE_MAX_KEY_SIZE];
		PGrnColumnNameEncode(data->desc->attrs[data->i]->attname.data,
							 columnName);
		PGrnCreateColumn(data->sourcesTable,
						 columnName,
						 flags,
						 range);
		PGrnWALCreateColumn(data->index,
							data->sourcesTable,
							columnName,
							flags,
							range);
	}
}

void
PGrnCreateLexicon(PGrnCreateData *data)
{
	grn_id typeID = GRN_ID_NIL;
	char lexiconName[GRN_TABLE_MAX_KEY_SIZE];
	grn_table_flags flags = GRN_OBJ_TABLE_PAT_KEY;
	grn_obj *type;
	grn_obj *lexicon;
	grn_obj *tokenizer = NULL;
	grn_obj *normalizer = NULL;

	switch (data->attributeTypeID)
	{
	case GRN_DB_TEXT:
	case GRN_DB_LONG_TEXT:
		typeID = GRN_DB_SHORT_TEXT;
		break;
	default:
		typeID = data->attributeTypeID;
		break;
	}

	snprintf(lexiconName, sizeof(lexiconName),
			 PGrnLexiconNameFormat, data->relNode, data->i);
	type = grn_ctx_at(ctx, typeID);
	lexicon = PGrnCreateTable(lexiconName, flags, type);
	GRN_PTR_PUT(ctx, data->lexicons, lexicon);

	if (data->forFullTextSearch ||
		data->forRegexpSearch ||
		data->forPrefixSearch)
	{
		const char *tokenizerName;
		const char *normalizerName = PGRN_DEFAULT_NORMALIZER;

		if (data->forRegexpSearch) {
			tokenizerName = "TokenRegexp";
		} else {
			tokenizerName = PGRN_DEFAULT_TOKENIZER;
		}

		PGrnApplyOptionValues(data->index, &tokenizerName, &normalizerName);

		if (data->forFullTextSearch || data->forRegexpSearch)
		{
			if (!PGrnIsNoneValue(tokenizerName))
			{
				tokenizer = PGrnLookup(tokenizerName, ERROR);
				grn_obj_set_info(ctx, lexicon, GRN_INFO_DEFAULT_TOKENIZER,
								 tokenizer);
			}
		}

		if (!PGrnIsNoneValue(normalizerName))
		{
			normalizer = PGrnLookup(normalizerName, ERROR);
			grn_obj_set_info(ctx, lexicon, GRN_INFO_NORMALIZER, normalizer);
		}
	}

	PGrnWALCreateTable(data->index,
					   lexiconName,
					   flags,
					   type,
					   tokenizer,
					   normalizer);
}

void
PGrnCreateIndexColumn(PGrnCreateData *data)
{
	char lexiconName[GRN_TABLE_MAX_KEY_SIZE];
	grn_obj *lexicon;
	grn_column_flags flags = GRN_OBJ_COLUMN_INDEX;

	snprintf(lexiconName, sizeof(lexiconName),
			 PGrnLexiconNameFormat, data->relNode, data->i);
	lexicon = PGrnLookup(lexiconName, ERROR);

	if (data->forFullTextSearch || data->forRegexpSearch)
		flags |= GRN_OBJ_WITH_POSITION;
	PGrnCreateColumn(lexicon,
					 PGrnIndexColumnName,
					 flags,
					 data->sourcesTable);
	PGrnWALCreateColumn(data->index,
						lexicon,
						PGrnIndexColumnName,
						flags,
						data->sourcesTable);
}
