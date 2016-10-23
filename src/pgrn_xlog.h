#pragma once

#include <postgres.h>
#include <utils/relcache.h>

#include <groonga.h>

typedef struct PGrnXLogData_ PGrnXLogData;

bool PGrnXLogGetEnabled(void);
void PGrnXLogEnable(void);
void PGrnXLogDisable(void);

PGrnXLogData *PGrnXLogStart(Relation index);
void PGrnXLogFinish(PGrnXLogData *data);
void PGrnXLogAbort(PGrnXLogData *data);

void PGrnXLogInsertStart(PGrnXLogData *data, size_t nColumns);
void PGrnXLogInsertFinish(PGrnXLogData *data);
void PGrnXLogInsertColumnStart(PGrnXLogData *data, const char *name);
void PGrnXLogInsertColumnFinish(PGrnXLogData *data);
void PGrnXLogInsertColumn(PGrnXLogData *data,
						  const char *name,
						  grn_obj *value);
