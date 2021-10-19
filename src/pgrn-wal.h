#pragma once

#include <postgres.h>
#include <utils/relcache.h>

#include <groonga.h>

typedef struct PGrnWALData_ PGrnWALData;

bool PGrnWALGetEnabled(void);
void PGrnWALEnable(void);
void PGrnWALDisable(void);

size_t PGrnWALGetMaxSize(void);
void PGrnWALSetMaxSize(size_t size);

PGrnWALData *PGrnWALStart(Relation index);
void PGrnWALFinish(PGrnWALData *data);
void PGrnWALAbort(PGrnWALData *data);

void PGrnWALInsertStart(PGrnWALData *data,
						grn_obj *table,
						size_t nColumns);
void PGrnWALInsertFinish(PGrnWALData *data);
void PGrnWALInsertColumnStart(PGrnWALData *data,
							  const char *name,
							  size_t nameSize);
void PGrnWALInsertColumnFinish(PGrnWALData *data);
void PGrnWALInsertColumn(PGrnWALData *data,
						 grn_obj *column,
						 grn_obj *value);
void PGrnWALInsertKeyRaw(PGrnWALData *data,
						 const void *key,
						 size_t keySize);
void PGrnWALInsertKey(PGrnWALData *data,
					  grn_obj *key);

void PGrnWALCreateTable(Relation index,
						const char *name,
						size_t nameSize,
						grn_table_flags flags,
						grn_obj *type,
						grn_obj *tokenizer,
						grn_obj *normalizers,
						grn_obj *tokenFilters);

void PGrnWALCreateColumn(Relation index,
						 grn_obj *table,
						 const char *name,
						 size_t nameSize,
						 grn_column_flags flags,
						 grn_obj *type);

void PGrnWALSetSourceIDs(Relation index,
						 grn_obj *column,
						 grn_obj *sourceIDs);

void PGrnWALRenameTable(Relation index,
						const char *name,
						size_t nameSize,
						const char *newName,
						size_t newNameSize);

void PGrnWALDelete(Relation index,
				   grn_obj *table,
				   const char *key,
				   size_t keySize);

int64_t PGrnWALApply(Relation index);
