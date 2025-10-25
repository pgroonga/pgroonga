#pragma once

#include "pgrn-compatible.h"

#include <nodes/execnodes.h>

#include <groonga.h>

typedef struct PGrnCreateData
{
	Relation heap;
	Relation index;
	IndexInfo *indexInfo;
	grn_obj *sourcesTable;
	grn_obj *sourcesCtidColumn;
	grn_obj *supplementaryTables;
	grn_obj *lexicons;
	TupleDesc desc;
	PGrnRelFileNumber relNumber;
	int i;
	bool forFullTextSearch;
	bool forRegexpSearch;
	bool forPrefixSearch;
	bool forSemanticSearch;
	grn_id attributeTypeID;
	unsigned char attributeFlags;
} PGrnCreateData;

void PGrnCreateSourcesTable(PGrnCreateData *data);
void PGrnCreateSourcesTableFinish(PGrnCreateData *data);
void PGrnCreateLexicon(PGrnCreateData *data);
void PGrnCreateDataColumn(PGrnCreateData *data);
void PGrnCreateIndexColumn(PGrnCreateData *data);
