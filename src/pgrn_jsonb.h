#pragma once

#include <postgres.h>
#include <access/skey.h>

#include "pgrn_create.h"
#include "pgrn_search.h"

void PGrnInitializeJSONB(void);
void PGrnFinalizeJSONB(void);

bool PGrnAttributeIsJSONB(Oid id);

void PGrnJSONBCreate(PGrnCreateData *data);

grn_obj *PGrnJSONBLookupValuesTable(Relation index,
									unsigned int nthAttribute,
									int errorLevel);
grn_obj *PGrnJSONBLookupPathsTable(Relation index,
								   unsigned int nthAttribute,
								   int errorLevel);
grn_obj *PGrnJSONBLookupTypesTable(Relation index,
								   unsigned int nthAttribute,
								   int errorLevel);
grn_obj *PGrnJSONBLookupFullTextSearchLexicon(Relation index,
											  unsigned int nthAttribute,
											  int errorLevel);
grn_obj *PGrnJSONBLookupStringLexicon(Relation index,
									  unsigned int nthAttribute,
									  int errorLevel);
grn_obj *PGrnJSONBLookupNumberLexicon(Relation index,
									  unsigned int nthAttribute,
									  int errorLevel);
grn_obj *PGrnJSONBLookupBooleanLexicon(Relation index,
									   unsigned int nthAttribute,
									   int errorLevel);
grn_obj *PGrnJSONBLookupSizeLexicon(Relation index,
									unsigned int nthAttribute,
									int errorLevel);

grn_obj *PGrnJSONBSetSource(Relation index, unsigned int i);

void PGrnJSONBInsert(Relation index,
					 grn_obj *sourcesTable,
					 grn_obj *sourcesCtidColumn,
					 Datum *values,
					 bool *isnull,
					 uint64_t packedCtid);

bool PGrnJSONBBuildSearchCondition(PGrnSearchData *data,
								   ScanKey key,
								   grn_obj *targetColumn);

typedef struct
{
	bool isJSONBAttribute;
	Relation index;
	grn_obj *sourcesTable;
	grn_obj *sourcesValuesColumn;
	grn_obj *valuesTable;
	grn_obj *valuesIndexColumn;
	grn_obj values;
	grn_obj valueMin;
	grn_obj valueMax;
	grn_id id;
} PGrnJSONBBulkDeleteData;

void PGrnJSONBBulkDeleteInit(PGrnJSONBBulkDeleteData *data);
void PGrnJSONBBulkDeleteRecord(PGrnJSONBBulkDeleteData *data);
void PGrnJSONBBulkDeleteFin(PGrnJSONBBulkDeleteData *data);

void PGrnJSONBRemoveUnusedTables(Oid relationFileNodeID);
