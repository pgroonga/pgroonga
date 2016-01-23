#pragma once

#include <groonga.h>

struct PGrnBuffers {
	grn_obj inspect;
};

grn_ctx PGrnContext;
struct PGrnBuffers PGrnBuffers;

void PGrnInitializeBuffers(void);
void PGrnFinalizeBuffers(void);
