#pragma once

#include <postgres.h>
#include <storage/bufpage.h>
#include <utils/rel.h>

#include <groonga.h>

void PGrnInitializeIndexStatus(void);

uint32_t PGrnIndexStatusGetMaxRecordSize(Relation index);
void PGrnIndexStatusSetMaxRecordSize(Relation index, uint32_t size);

void PGrnIndexStatusGetWALAppliedPosition(Relation index,
										  BlockNumber *block,
										  LocationIndex *offset);
void PGrnIndexStatusSetWALAppliedPosition(Relation index,
										  BlockNumber block,
										  LocationIndex offset);
