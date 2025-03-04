#include <postgres.h>

#include <nodes/extensible.h>
#include <optimizer/pathnode.h>
#include <optimizer/paths.h>
#include <optimizer/restrictinfo.h>

bool PGrnCustomScanGetEnabled(void);
void PGrnCustomScanEnable(void);
void PGrnCustomScanDisable(void);

void PGrnInitializeCustomScan(void);
