#include "pgroonga.h"

#include "pgrn_column_name.h"
#include "pgrn_create.h"
#include "pgrn_global.h"
#include "pgrn_groonga.h"
#include "pgrn_options.h"
#include "pgrn_value.h"

static grn_ctx *ctx = &PGrnContext;

void
PGrnCreateSourcesCtidColumn(PGrnCreateData *data)
{
	data->sourcesCtidColumn = PGrnCreateColumn(data->sourcesTable,
											   PGrnSourcesCtidColumnName,
											   GRN_OBJ_COLUMN_SCALAR,
											   grn_ctx_at(ctx, GRN_DB_UINT64));
}

void
PGrnCreateSourcesTable(PGrnCreateData *data)
{
	char sourcesTableName[GRN_TABLE_MAX_KEY_SIZE];

	snprintf(sourcesTableName, sizeof(sourcesTableName),
			 PGrnSourcesTableNameFormat, data->relNode);
	data->sourcesTable = PGrnCreateTable(sourcesTableName,
										 GRN_OBJ_TABLE_NO_KEY,
										 NULL);

	PGrnCreateSourcesCtidColumn(data);
}

void
PGrnCreateDataColumn(PGrnCreateData *data)
{
	grn_obj_flags flags = 0;

	if (data->attributeFlags & GRN_OBJ_VECTOR)
	{
		flags |= GRN_OBJ_COLUMN_VECTOR;
	}
	else
	{
		flags |= GRN_OBJ_COLUMN_SCALAR;

		if (PGrnIsLZ4Available)
		{
			switch (data->attributeTypeID)
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
						 grn_ctx_at(ctx, data->attributeTypeID));
	}
}

void
PGrnCreateIndexColumn(PGrnCreateData *data)
{
	grn_id typeID = GRN_ID_NIL;
	char lexiconName[GRN_TABLE_MAX_KEY_SIZE];
	grn_obj *lexicon;

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
	lexicon = PGrnCreateTable(lexiconName,
							  GRN_OBJ_TABLE_PAT_KEY,
							  grn_ctx_at(ctx, typeID));
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
				grn_obj_set_info(ctx, lexicon, GRN_INFO_DEFAULT_TOKENIZER,
								 PGrnLookup(tokenizerName, ERROR));
			}
		}

		if (!PGrnIsNoneValue(normalizerName))
		{
			grn_obj_set_info(ctx, lexicon, GRN_INFO_NORMALIZER,
							 PGrnLookup(normalizerName, ERROR));
		}
	}

	{
		grn_obj_flags flags = GRN_OBJ_COLUMN_INDEX;
		if (data->forFullTextSearch || data->forRegexpSearch)
			flags |= GRN_OBJ_WITH_POSITION;
		PGrnCreateColumn(lexicon,
						 PGrnIndexColumnName,
						 flags,
						 data->sourcesTable);
	}
}
