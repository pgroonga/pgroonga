#pragma once

#include <postgres.h>
#include <utils/rel.h>

#include <groonga.h>

bool PGrnIsLZ4Available;

void PGrnInitializeGroongaInformation(void);

const char *PGrnInspect(grn_obj *object);

int PGrnRCToPgErrorCode(grn_rc rc);
grn_bool PGrnCheck(const char *message);

grn_obj *PGrnLookup(const char *name, int errorLevel);
grn_obj *PGrnLookupWithSize(const char *name,
							size_t nameSize,
							int errorLevel);
grn_obj *PGrnLookupColumn(grn_obj *table, const char *name, int errorLevel);
grn_obj *PGrnLookupColumnWithSize(grn_obj *table,
								  const char *name,
								  size_t nameSize,
								  int errorLevel);
grn_obj *PGrnLookupSourcesTable(Relation index, int errorLevel);
grn_obj *PGrnLookupSourcesCtidColumn(Relation index, int errorLevel);
grn_obj *PGrnLookupLexicon(Relation index,
						   unsigned int nthAttribute,
						   int errorLevel);
grn_obj *PGrnLookupIndexColumn(Relation index,
							   unsigned int nthAttribute,
							   int errorLevel);

grn_obj *PGrnCreateTable(const char *name,
						 grn_table_flags flags,
						 grn_obj *type);
grn_obj *PGrnCreateTableWithSize(const char *name,
								 size_t nameSize,
								 grn_table_flags flags,
								 grn_obj *type);
grn_obj *PGrnCreateColumn(grn_obj *table,
						  const char*name,
						  grn_column_flags flags,
						  grn_obj *type);
grn_obj *PGrnCreateColumnWithSize(grn_obj *table,
								  const char*name,
								  size_t nameSize,
								  grn_column_flags flags,
								  grn_obj *type);

void PGrnIndexColumnSetSource(grn_obj *indexColumn, grn_obj *source);

bool PGrnRemoveObject(const char *name);

void PGrnFlushObject(grn_obj *object, bool recursive);
