#include "pgroonga.h"

#include "pgrn_column_name.h"
#include "pgrn_global.h"
#include "pgrn_groonga.h"

bool PGrnIsLZ4Available;

static grn_ctx *ctx = &PGrnContext;
static struct PGrnBuffers *buffers = &PGrnBuffers;

void
PGrnInitializeGroongaInformation(void)
{
	grn_obj grnIsSupported;

	GRN_BOOL_INIT(&grnIsSupported, 0);
	grn_obj_get_info(ctx, NULL, GRN_INFO_SUPPORT_LZ4, &grnIsSupported);
	PGrnIsLZ4Available = (GRN_BOOL_VALUE(&grnIsSupported));
	GRN_OBJ_FIN(ctx, &grnIsSupported);
}

const char *
PGrnInspect(grn_obj *object)
{
	grn_obj *buffer = &(PGrnBuffers.inspect);

	GRN_BULK_REWIND(buffer);
	grn_inspect(ctx, buffer, object);
	GRN_TEXT_PUTC(ctx, buffer, '\0');
	return GRN_TEXT_VALUE(buffer);
}

int
PGrnRCToPgErrorCode(grn_rc rc)
{
	int errorCode = ERRCODE_SYSTEM_ERROR;

	/* TODO: Fill me. */
	switch (rc)
	{
	case GRN_NO_SUCH_FILE_OR_DIRECTORY:
		errorCode = ERRCODE_IO_ERROR;
		break;
	case GRN_INPUT_OUTPUT_ERROR:
		errorCode = ERRCODE_IO_ERROR;
		break;
	case GRN_INVALID_ARGUMENT:
		errorCode = ERRCODE_INVALID_PARAMETER_VALUE;
		break;
	default:
		break;
	}

	return errorCode;
}

grn_bool
PGrnCheck(const char *message)
{
	if (ctx->rc == GRN_SUCCESS)
		return GRN_TRUE;

	ereport(ERROR,
			(errcode(PGrnRCToPgErrorCode(ctx->rc)),
			 errmsg("pgroonga: %s: %s", message, ctx->errbuf)));
	return GRN_FALSE;
}

grn_obj *
PGrnLookup(const char *name, int errorLevel)
{
	grn_obj *object = grn_ctx_get(ctx, name, strlen(name));
	if (!object)
		ereport(errorLevel,
				(errcode(ERRCODE_INVALID_NAME),
				 errmsg("pgroonga: object isn't found: <%s>", name)));
	return object;
}

grn_obj *
PGrnLookupColumn(grn_obj *table, const char *name, int errorLevel)
{
	char columnName[GRN_TABLE_MAX_KEY_SIZE];
	size_t columnNameSize;
	grn_obj *column;

	columnNameSize = PGrnColumnNameEncode(name, columnName);
	column = grn_obj_column(ctx, table, columnName, columnNameSize);
	if (!column)
	{
		char tableName[GRN_TABLE_MAX_KEY_SIZE];
		int tableNameSize;

		tableNameSize = grn_obj_name(ctx, table, tableName, sizeof(tableName));
		ereport(errorLevel,
				(errcode(ERRCODE_INVALID_NAME),
				 errmsg("pgroonga: column isn't found: <%.*s>:<%s>",
						tableNameSize, tableName,
						name)));
	}

	return column;
}

grn_obj *
PGrnLookupSourcesTable(Relation index, int errorLevel)
{
	char name[GRN_TABLE_MAX_KEY_SIZE];

	snprintf(name, sizeof(name),
			 PGrnSourcesTableNameFormat,
			 index->rd_node.relNode);
	return PGrnLookup(name, errorLevel);
}

grn_obj *
PGrnLookupSourcesCtidColumn(Relation index, int errorLevel)
{
	char name[GRN_TABLE_MAX_KEY_SIZE];

	snprintf(name, sizeof(name),
			 PGrnSourcesTableNameFormat "." PGrnSourcesCtidColumnName,
			 index->rd_node.relNode);
	return PGrnLookup(name, errorLevel);
}

grn_obj *
PGrnLookupLexicon(Relation index, unsigned int nthAttribute, int errorLevel)
{
	char name[GRN_TABLE_MAX_KEY_SIZE];

	snprintf(name, sizeof(name),
			 PGrnLexiconNameFormat,
			 index->rd_node.relNode,
			 nthAttribute);
	return PGrnLookup(name, errorLevel);
}

grn_obj *
PGrnLookupIndexColumn(Relation index, unsigned int nthAttribute, int errorLevel)
{
	char name[GRN_TABLE_MAX_KEY_SIZE];

	snprintf(name, sizeof(name),
			 PGrnLexiconNameFormat ".%s",
			 index->rd_node.relNode,
			 nthAttribute,
			 PGrnIndexColumnName);
	return PGrnLookup(name, errorLevel);
}

grn_obj *
PGrnCreateTable(const char *name,
				grn_obj_flags flags,
				grn_obj *type)
{
	grn_obj	*table;
	unsigned int nameSize = 0;

	if (name)
	{
		nameSize = strlen(name);
		flags |= GRN_OBJ_PERSISTENT;
	}

	table = grn_table_create(ctx,
							 name, nameSize, NULL,
							 flags,
							 type,
							 NULL);
	PGrnCheck("pgroonga: failed to create table");

	return table;
}

grn_obj *
PGrnCreateColumn(grn_obj	*table,
				 const char *name,
				 grn_obj_flags flags,
				 grn_obj	*type)
{
	grn_obj *column;

    column = grn_column_create(ctx, table,
							   name, strlen(name), NULL,
							   GRN_OBJ_PERSISTENT | flags,
							   type);
	PGrnCheck("pgroonga: failed to create column");

	return column;
}

void
PGrnIndexColumnSetSource(grn_obj *indexColumn, grn_obj *source)
{
	grn_id sourceID;

	GRN_BULK_REWIND(&(buffers->sourceIDs));

	sourceID = grn_obj_id(ctx, source);
	GRN_RECORD_PUT(ctx, &(buffers->sourceIDs), sourceID);

	grn_obj_set_info(ctx, indexColumn, GRN_INFO_SOURCE, &(buffers->sourceIDs));
}

bool
PGrnRemoveObject(const char *name)
{
	grn_obj *object = grn_ctx_get(ctx, name, strlen(name));

	if (object)
	{
		grn_obj_remove(ctx, object);
		return true;
	}
	else
	{
		return false;
	}
}

void
PGrnFlushObject(grn_obj *object, bool recursive)
{
	grn_rc rc;
	char name[GRN_TABLE_MAX_KEY_SIZE];
	int name_size;

	if (recursive)
	{
		rc = grn_obj_flush_recursive(ctx, object);
	}
	else
	{
		rc = grn_obj_flush(ctx, object);
	}
	if (rc == GRN_SUCCESS)
		return;

	name_size = grn_obj_name(ctx, object, name, GRN_TABLE_MAX_KEY_SIZE);
	ereport(ERROR,
			(errcode(PGrnRCToPgErrorCode(rc)),
			 errmsg("pgroonga: failed to flush: <%.*s>: %s",
					name_size, name,
					ctx->errbuf)));
}
