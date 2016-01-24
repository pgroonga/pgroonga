#pragma once

#include <postgres.h>
#include <utils/rel.h>

#include <groonga.h>

typedef struct PGrnCreateData
{
	Relation index;
	grn_obj *sourcesTable;
	grn_obj *sourcesCtidColumn;
	grn_obj *supplementaryTables;
	grn_obj *lexicons;
	unsigned int i;
	TupleDesc desc;
	Oid relNode;
	bool forFullTextSearch;
	bool forRegexpSearch;
	grn_id attributeTypeID;
	unsigned char attributeFlags;
} PGrnCreateData;

void PGrnCreateSourcesCtidColumn(PGrnCreateData *data);
void PGrnCreateSourcesTable(PGrnCreateData *data);
void PGrnCreateDataColumn(PGrnCreateData *data);
void PGrnCreateIndexColumn(PGrnCreateData *data);
