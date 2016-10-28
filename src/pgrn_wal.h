#pragma once

#include <postgres.h>
#include <utils/relcache.h>

#include <groonga.h>

typedef struct PGrnWALData_ PGrnWALData;

bool PGrnWALGetEnabled(void);
void PGrnWALEnable(void);
void PGrnWALDisable(void);

PGrnWALData *PGrnWALStart(Relation index);
void PGrnWALFinish(PGrnWALData *data);
void PGrnWALAbort(PGrnWALData *data);

void PGrnWALInsertStart(PGrnWALData *data, size_t nColumns);
void PGrnWALInsertFinish(PGrnWALData *data);
void PGrnWALInsertColumnStart(PGrnWALData *data, const char *name);
void PGrnWALInsertColumnFinish(PGrnWALData *data);
void PGrnWALInsertColumn(PGrnWALData *data,
						 const char *name,
						 grn_obj *value);

void PGrnWALCreateTable(Relation index,
						const char *name,
						grn_table_flags flags,
						grn_obj *type,
						grn_obj *tokenizer,
						grn_obj *normalizer);

void PGrnWALCreateColumn(Relation index,
						 grn_obj *table,
						 const char *name,
						 grn_column_flags flags,
						 grn_obj *type);

void PGrnWALSetSource(Relation index,
					  grn_obj *column,
					  grn_obj *source);
void PGrnWALSetSources(Relation index,
					   grn_obj *column,
					   grn_obj *sources);

void PGrnWALApply(Relation index);
