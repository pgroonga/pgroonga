#pragma once

#include <postgres.h>
#include <utils/rel.h>

void PGrnInitializeOptions(void);

void PGrnApplyOptionValues(Relation index,
						   const char **tokenizerName,
						   const char **normalizerName);
