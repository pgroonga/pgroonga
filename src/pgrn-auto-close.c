#include "pgroonga.h"

#include "pgrn-auto-close.h"
#include "pgrn-compatible.h"
#include "pgrn-global.h"

static grn_ctx *ctx = &PGrnContext;
static grn_hash *usingIndexes = NULL;

void
PGrnInitializeAutoClose(void)
{
	usingIndexes = grn_hash_create(ctx,
								   NULL,
								   sizeof(Oid),
								   sizeof(Oid),
								   0);
}

void
PGrnFinalizeAutoClose(void)
{
	if (usingIndexes)
	{
		grn_hash_close(ctx, usingIndexes);
		usingIndexes = NULL;
	}
}

static void
PGrnAutoCloseCloseUnusedObjects(Oid nodeID)
{
	char *prefixes[] = {
		PGrnLexiconNamePrefix "%u_",
		PGrnJSONValueLexiconNamePrefix "FullTextSearch%u_",
		PGrnJSONValueLexiconNamePrefix "String%u_",
		PGrnJSONValueLexiconNamePrefix "Number%u_",
		PGrnJSONValueLexiconNamePrefix "Boolean%u_",
		PGrnJSONValueLexiconNamePrefix "Size%u_",
		PGrnJSONTypesTableNamePrefix "%u_",
		PGrnJSONValuesTableNamePrefix "%u_",
		PGrnJSONPathsTableNamePrefix "%u_",
		PGrnBuildingSourcesTableNamePrefix "%u",
		PGrnSourcesTableNamePrefix "%u",
	};
	size_t i;
	const size_t n_prefixes = sizeof(prefixes) / sizeof(*prefixes);
	grn_obj *db;

	db = grn_ctx_db(ctx);
	for (i = 0; i < n_prefixes; i++)
	{
		char prefix[GRN_TABLE_MAX_KEY_SIZE];

		snprintf(prefix, sizeof(prefix),
				 prefixes[i], nodeID);
		GRN_TABLE_EACH_BEGIN_MIN(ctx,
								 db,
								 cursor,
								 id,
								 prefix,
								 strlen(prefix),
								 GRN_CURSOR_PREFIX | GRN_CURSOR_DESCENDING)
		{
			void *key;
			const char *name;
			int name_size;
			grn_obj *object;

			if (!grn_ctx_is_opened(ctx, id))
				continue;

			name_size = grn_table_cursor_get_key(ctx, cursor, &key);
			name = key;
			GRN_LOG(ctx, GRN_LOG_DEBUG,
					"pgroonga: auto-close: <%.*s>",
					name_size, name);

			object = grn_ctx_at(ctx, id);
			grn_obj_close(ctx, object);
		} GRN_TABLE_EACH_END(ctx, cursor);
	}
}

void
PGrnAutoCloseUseIndex(Relation index)
{
	grn_id id;
	void *value;

	if (!usingIndexes)
		return;

	id = grn_hash_get(ctx,
					  usingIndexes,
					  &(index->rd_id),
					  sizeof(index->rd_id),
					  &value);
	if (id == GRN_ID_NIL)
	{
		id = grn_hash_add(ctx,
						  usingIndexes,
						  &(index->rd_id),
						  sizeof(index->rd_id),
						  &value,
						  NULL);
		if (id == GRN_ID_NIL)
			return;
		*((PGrnRelFileNumber *)value) = PGRN_RELATION_GET_LOCATOR_NUMBER(index);
	}
	else
	{
		Oid currentNodeID = *((Oid *)value);
		if (PGRN_RELATION_GET_LOCATOR_NUMBER(index) == currentNodeID)
			return;
		PGrnAutoCloseCloseUnusedObjects(currentNodeID);
		*((PGrnRelFileNumber *)value) = PGRN_RELATION_GET_LOCATOR_NUMBER(index);
	}
}
