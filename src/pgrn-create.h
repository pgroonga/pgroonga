#pragma once

#include <postgres.h>
#include <utils/rel.h>

#include <groonga.h>

typedef struct PGrnCreateData
{
	Relation index;
	grn_obj *sourcesTable;
	grn_obj *supplementaryTables;
	grn_obj *lexicons;
	TupleDesc desc;
	Oid relNode;
	unsigned int i;
	bool forFullTextSearch;
	bool forRegexpSearch;
	bool forPrefixSearch;
	grn_id attributeTypeID;
	unsigned char attributeFlags;
} PGrnCreateData;

void PGrnCreateSourcesTable(PGrnCreateData *data);
void PGrnCreateSourcesTableFinish(PGrnCreateData *data);
void PGrnCreateLexicon(PGrnCreateData *data);
void PGrnCreateDataColumn(PGrnCreateData *data);
void PGrnCreateIndexColumn(PGrnCreateData *data);
