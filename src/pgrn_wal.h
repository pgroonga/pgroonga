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

void PGrnWALApply(Relation index);
