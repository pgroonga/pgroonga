#pragma once

#include <postgres.h>
#include <utils/rel.h>

#include <groonga.h>

bool PGrnIsLZ4Available;

void PGrnInitializeGroongaInformation(void);

const char *PGrnInspect(grn_obj *object);
const char *PGrnInspectName(grn_obj *object);

int PGrnRCToPgErrorCode(grn_rc rc);
grn_bool PGrnCheck(const char *format,
				   ...) GRN_ATTRIBUTE_PRINTF(1);

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

grn_obj *PGrnCreateTable(Relation index,
						 const char *name,
						 grn_table_flags flags,
						 grn_obj *type,
						 grn_obj *tokenizer,
						 grn_obj *normalizer);
grn_obj *PGrnCreateTableWithSize(Relation index,
								 const char *name,
								 size_t nameSize,
								 grn_table_flags flags,
								 grn_obj *type,
								 grn_obj *tokenizer,
								 grn_obj *normalizer);
grn_obj *PGrnCreateColumn(Relation index,
						  grn_obj *table,
						  const char*name,
						  grn_column_flags flags,
						  grn_obj *type);
grn_obj *PGrnCreateColumnWithSize(Relation index,
								  grn_obj *table,
								  const char*name,
								  size_t nameSize,
								  grn_column_flags flags,
								  grn_obj *type);

void PGrnIndexColumnSetSource(Relation index,
							  grn_obj *indexColumn,
							  grn_obj *source);
void PGrnIndexColumnSetSourceIDs(Relation index,
								 grn_obj *indexColumn,
								 grn_obj *sourceIDs);

bool PGrnRemoveObject(const char *name);
bool PGrnRemoveObjectWithSize(const char *name, size_t nameSize);

void PGrnFlushObject(grn_obj *object, bool recursive);
