#pragma once

#include <postgres.h>
#include <utils/rel.h>

#include <groonga.h>

void PGrnInitializeAlias(void);

void PGrnAliasAdd(Relation index);
void PGrnAliasDeleteRaw(Oid indexFileNodeID);
void PGrnAliasDelete(Relation index);
