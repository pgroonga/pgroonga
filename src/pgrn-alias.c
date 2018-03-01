#include "pgroonga.h"

#include "pgrn-global.h"
#include "pgrn-groonga.h"
#include "pgrn-alias.h"
#include "pgrn-wal.h"

static grn_ctx *ctx = &PGrnContext;
static struct PGrnBuffers *buffers = &PGrnBuffers;

#define TABLE_NAME "Aliases"
#define TABLE_NAME_SIZE (sizeof(TABLE_NAME) - 1)
#define REAL_NAME_COLUMN_NAME "real_name"
#define FULL_REAL_NAME_COLUMN_NAME \
	(TABLE_NAME "." REAL_NAME_COLUMN_NAME)
#define FULL_REAL_NAME_COLUMN_NAME_SIZE \
	(sizeof(FULL_REAL_NAME_COLUMN_NAME) - 1)
#define ALIAS_CONFIG_KEY "alias.column"
#define ALIAS_CONFIG_KEY_SIZE (sizeof(ALIAS_CONFIG_KEY) - 1)

static bool
PGrnInitializeAliasUpdateConfigNeeded(void)
{
	const char *value = NULL;
	uint32_t valueSize = 0;

	grn_config_get(ctx,
				   ALIAS_CONFIG_KEY,
				   ALIAS_CONFIG_KEY_SIZE,
				   &value,
				   &valueSize);

	if (!value)
		return true;

	if (valueSize != FULL_REAL_NAME_COLUMN_NAME_SIZE)
		return true;

	if (memcmp(value, FULL_REAL_NAME_COLUMN_NAME, valueSize) != 0)
		return true;

	return false;
}

static void
PGrnInitializeAliasUpdateConfig(void)
{
	if (!PGrnInitializeAliasUpdateConfigNeeded())
		return;

	grn_config_set(ctx,
				   ALIAS_CONFIG_KEY,
				   ALIAS_CONFIG_KEY_SIZE,
				   FULL_REAL_NAME_COLUMN_NAME,
				   FULL_REAL_NAME_COLUMN_NAME_SIZE);
}

void
PGrnInitializeAlias(void)
{
	grn_obj *table;

	table = grn_ctx_get(ctx,
						TABLE_NAME,
						TABLE_NAME_SIZE);
	if (!table)
	{
		table = PGrnCreateTableWithSize(NULL,
										TABLE_NAME,
										TABLE_NAME_SIZE,
										GRN_OBJ_TABLE_HASH_KEY,
										grn_ctx_at(ctx, GRN_DB_SHORT_TEXT),
										NULL,
										NULL,
										NULL);
	}

	if (!grn_ctx_get(ctx, TABLE_NAME "." REAL_NAME_COLUMN_NAME, -1))
	{
		PGrnCreateColumn(NULL,
						 table,
						 REAL_NAME_COLUMN_NAME,
						 GRN_OBJ_COLUMN_SCALAR,
						 grn_ctx_at(ctx, GRN_DB_SHORT_TEXT));
	}

	PGrnInitializeAliasUpdateConfig();
}

static grn_obj *
PGrnAliasLookupTable(void)
{
	return PGrnLookupWithSize(TABLE_NAME,
							  TABLE_NAME_SIZE,
							  ERROR);
}

static grn_obj *
PGrnAliasLookupColumn(void)
{
	return PGrnLookupWithSize(FULL_REAL_NAME_COLUMN_NAME,
							  FULL_REAL_NAME_COLUMN_NAME_SIZE,
							  ERROR);
}

static void
PGrnAliasFillOldNameRaw(Oid indexFileNodeID,
						char *name,
						size_t nameSize)
{
	grn_snprintf(name, nameSize, nameSize,
				 PGrnSourcesTableNameFormat "." PGrnSourcesCtidColumnName,
				 indexFileNodeID);
}

static void
PGrnAliasFillNewNameRaw(Oid indexFileNodeID, char *name, size_t nameSize)
{
	grn_snprintf(name, nameSize, nameSize,
				 PGrnSourcesTableNameFormat "._key",
				 indexFileNodeID);
}

void
PGrnAliasAdd(Relation index)
{
	grn_obj *table;
	grn_obj *column;
	char old[GRN_TABLE_MAX_KEY_SIZE];
	char new[GRN_TABLE_MAX_KEY_SIZE];
	grn_id id;

	table = PGrnAliasLookupTable();
	column = PGrnAliasLookupColumn();
	PGrnAliasFillOldNameRaw(index->rd_node.relNode, old, sizeof(old));
	PGrnAliasFillNewNameRaw(index->rd_node.relNode, new, sizeof(new));

	id = grn_table_add(ctx, table, old, strlen(old), NULL);
	if (id == GRN_ID_NIL)
	{
		PGrnCheck("alias: failed to add entry: <%s>", old);
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_NAME),
				 errmsg("pgroonga: alias: failed to add entry: <%s>",
						old)));
	}

	{
		grn_obj *newValue = &(buffers->general);
		grn_obj_reinit(ctx,
					   newValue,
					   GRN_DB_SHORT_TEXT,
					   GRN_OBJ_DO_SHALLOW_COPY);
		GRN_TEXT_SET(ctx, newValue, new, strlen(new));
		grn_obj_set_value(ctx, column, id, newValue, GRN_OBJ_SET);
		PGrnCheck("alias: failed to set entry: <%s>(%u) -> <%s>",
				  old, id, new);
	}
}

void
PGrnAliasDeleteRaw(Oid indexFileNodeID)
{
	grn_obj *table;
	char old[GRN_TABLE_MAX_KEY_SIZE];
	grn_id id;

	table = PGrnAliasLookupTable();
	PGrnAliasFillOldNameRaw(indexFileNodeID, old, sizeof(old));

	id = grn_table_get(ctx, table, old, strlen(old));
	if (id == GRN_ID_NIL)
		return;

	grn_table_delete(ctx, table, old, strlen(old));
	PGrnCheck("alias: failed to delete entry: <%s>", old);
}

void
PGrnAliasDelete(Relation index)
{
	PGrnAliasDeleteRaw(index->rd_node.relNode);
}
