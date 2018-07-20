#pragma once

#include <groonga.h>

void PGrnInitializeAutoClose(void);
void PGrnFinalizeAutoClose(void);
void PGrnAutoCloseUseIndex(Relation index);
