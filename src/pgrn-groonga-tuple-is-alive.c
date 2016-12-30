#include "pgroonga.h"

#include "pgrn-ctid.h"
#include "pgrn-global.h"
#include "pgrn-groonga-tuple-is-alive.h"
#include "pgrn-pg.h"

#include <groonga/plugin.h>

#include <storage/lmgr.h>

static Oid
sources_table_to_file_node_id(grn_ctx *ctx, grn_obj *table)
{
	char name[GRN_TABLE_MAX_KEY_SIZE];
	int name_size;

	name_size = grn_obj_name(ctx, table, name, GRN_TABLE_MAX_KEY_SIZE);
	name[name_size] = '\0';
	return strtol(name + PGrnSourcesTableNamePrefixLength, NULL, 10);
}

static grn_obj *
func_pgroonga_tuple_is_alive(grn_ctx *ctx,
							 int nargs,
							 grn_obj **args,
							 grn_user_data *user_data)
{
	grn_obj *is_alive;
	grn_bool is_alive_raw = GRN_FALSE;
	grn_obj *condition = NULL;
	grn_obj *variable;
	grn_obj *table;
	grn_obj *packed_ctid;
	grn_obj casted_packed_ctid;
	grn_rc rc;

	grn_proc_get_info(ctx, user_data, NULL, NULL, &condition);
	if (!condition)
	{
		GRN_PLUGIN_ERROR(ctx,
						 GRN_INVALID_ARGUMENT,
						 "pgroonga_tuple_is_alive(): condition is missing");
		goto exit;
	}

	variable = grn_expr_get_var_by_offset(ctx, condition, 0);
	if (!variable)
	{
		GRN_PLUGIN_ERROR(ctx,
						 GRN_INVALID_ARGUMENT,
						 "pgroonga_tuple_is_alive(): variable is missing");
		goto exit;
	}

	table = grn_ctx_at(ctx, variable->header.domain);
	if (!table)
	{
		GRN_PLUGIN_ERROR(ctx,
						 GRN_INVALID_ARGUMENT,
						 "pgroonga_tuple_is_alive(): table isn't found: <%u>",
						 variable->header.domain);
		goto exit;
	}

	if (nargs != 1)
	{
		GRN_PLUGIN_ERROR(ctx,
						 GRN_INVALID_ARGUMENT,
						 "pgroonga_tuple_is_alive(): must specify ctid");
		goto exit;
	}

	packed_ctid = args[0];
	GRN_UINT64_INIT(&casted_packed_ctid, 0);
	rc = grn_obj_cast(ctx, packed_ctid, &casted_packed_ctid, GRN_FALSE);
	if (rc != GRN_SUCCESS)
	{
		grn_obj inspected_packed_ctid;

		GRN_TEXT_INIT(&inspected_packed_ctid, 0);
		grn_inspect(ctx, &inspected_packed_ctid, packed_ctid);
		GRN_PLUGIN_ERROR(ctx,
						 rc,
						 "pgroonga_tuple_is_alive(): "
						 "invalid packed ctid: <%.*s>",
						 (int)GRN_TEXT_LEN(&inspected_packed_ctid),
						 GRN_TEXT_VALUE(&inspected_packed_ctid));
		GRN_OBJ_FIN(ctx, &inspected_packed_ctid);
		goto exit;
	}

	{
		Oid file_node_id;
		Relation pg_index;
		Oid pg_index_id;
		LOCKMODE lock_mode = AccessShareLock;

		file_node_id = sources_table_to_file_node_id(ctx, table);
		pg_index = PGrnPGResolveFileNodeID(file_node_id,
										   &pg_index_id,
										   lock_mode);
		if (RelationIsValid(pg_index))
		{
			Relation pg_table;
			ItemPointerData ctid;

			pg_table = RelationIdGetRelation(pg_index->rd_index->indrelid);
			ctid = PGrnCtidUnpack(GRN_UINT64_VALUE(&casted_packed_ctid));
			is_alive_raw = PGrnCtidIsAlive(pg_table, &ctid);
			RelationClose(pg_table);
			RelationClose(pg_index);
			UnlockRelationOid(pg_index_id, lock_mode);
		}
	}
	GRN_OBJ_FIN(ctx, &casted_packed_ctid);

exit:
	is_alive = grn_plugin_proc_alloc(ctx, user_data, GRN_DB_BOOL, 0);
	if (is_alive)
	{
		GRN_BOOL_SET(ctx, is_alive, is_alive_raw);
	}
	return is_alive;
}

static grn_rc
selector_pgroonga_tuple_is_alive(grn_ctx *ctx,
								 grn_obj *table,
								 grn_obj *index,
								 int nargs,
								 grn_obj **args,
								 grn_obj *res,
								 grn_operator op)
{
  Oid file_node_id;
  Relation pg_index;
  Oid pg_index_id;
  LOCKMODE lock_mode = AccessShareLock;

  switch (op)
  {
  case GRN_OP_AND:
  case GRN_OP_OR:
	  break;
  default:
	  return GRN_FUNCTION_NOT_IMPLEMENTED;
  }

  file_node_id = sources_table_to_file_node_id(ctx, table);
  pg_index = PGrnPGResolveFileNodeID(file_node_id, &pg_index_id, lock_mode);
  if (RelationIsValid(pg_index))
  {
	  Relation pg_table;
	  grn_obj *ctid_accessor;
	  grn_obj packed_ctid;

	  pg_table = RelationIdGetRelation(pg_index->rd_index->indrelid);
	  GRN_UINT64_INIT(&packed_ctid, 0);
	  if (op == GRN_OP_AND)
	  {
		  ctid_accessor = grn_obj_column(ctx, res, "ctid", strlen("ctid"));
		  GRN_TABLE_EACH_BEGIN(ctx, res, cursor, id)
		  {
			  ItemPointerData ctid;

			  GRN_BULK_REWIND(&packed_ctid);
			  grn_obj_get_value(ctx, ctid_accessor, id, &packed_ctid);
			  ctid = PGrnCtidUnpack(GRN_UINT64_VALUE(&packed_ctid));
			  if (!PGrnCtidIsAlive(pg_table, &ctid))
				  grn_table_cursor_delete(ctx, cursor);
		  }
		  GRN_TABLE_EACH_END(ctx, cursor);
	  }
	  else
	  {
		  grn_posting posting;

		  ctid_accessor = grn_obj_column(ctx, table, "ctid", strlen("ctid"));
		  memset(&posting, 0, sizeof(grn_posting));
		  GRN_TABLE_EACH_BEGIN(ctx, table, cursor, id)
		  {
			  ItemPointerData ctid;

			  GRN_BULK_REWIND(&packed_ctid);
			  grn_obj_get_value(ctx, ctid_accessor, id, &packed_ctid);
			  ctid = PGrnCtidUnpack(GRN_UINT64_VALUE(&packed_ctid));
			  if (PGrnCtidIsAlive(pg_table, &ctid))
			  {
				  grn_rc grn_ii_posting_add(grn_ctx *ctx,
											grn_posting *pos,
											grn_hash *s,
											grn_operator op);
				  posting.rid = id;
				  grn_ii_posting_add(ctx, &posting, (grn_hash *)res, op);
			  }
		  }
		  GRN_TABLE_EACH_END(ctx, cursor);
		  /* TODO: Enable it when we support GRN_OP_AND_NOT and GRN_OP_ADJUST. */
		  /* grn_ii_resolve_sel_and(ctx, res, op); */
	  }
	  grn_obj_unlink(ctx, ctid_accessor);
	  GRN_OBJ_FIN(ctx, &packed_ctid);
	  RelationClose(pg_table);
	  RelationClose(pg_index);
	  UnlockRelationOid(pg_index_id, lock_mode);
  }
  else
  {
	  if (op == GRN_OP_AND)
	  {
		  GRN_TABLE_EACH_BEGIN(ctx, res, cursor, id)
		  {
			  grn_table_cursor_delete(ctx, cursor);
		  }
		  GRN_TABLE_EACH_END(ctx, cursor);
	  }
	  else
	  {
		  grn_posting posting;

		  memset(&posting, 0, sizeof(grn_posting));
		  GRN_TABLE_EACH_BEGIN(ctx, table, cursor, id)
		  {
			  grn_rc grn_ii_posting_add(grn_ctx *ctx,
										grn_posting *pos,
										grn_hash *s,
										grn_operator op);
			  posting.rid = id;
			  grn_ii_posting_add(ctx, &posting, (grn_hash *)res, op);
		  }
		  GRN_TABLE_EACH_END(ctx, cursor);
		  /* TODO: Enable it when we support GRN_OP_AND_NOT and GRN_OP_ADJUST. */
		  /* grn_ii_resolve_sel_and(ctx, res, op); */
	  }
  }

  return ctx->rc;
}

void
PGrnInitializeGroongaTupleIsAlive(void)
{
	grn_ctx *ctx = &PGrnContext;
	grn_obj *proc;

	proc = grn_proc_create(ctx,
						   "pgroonga_tuple_is_alive", -1,
						   GRN_PROC_FUNCTION,
						   func_pgroonga_tuple_is_alive,
						   NULL,
						   NULL,
						   0,
						   NULL);
    grn_proc_set_selector(ctx, proc, selector_pgroonga_tuple_is_alive);
    grn_proc_set_selector_operator(ctx, proc, GRN_OP_NOP);
}
