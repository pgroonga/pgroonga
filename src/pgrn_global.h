#pragma once

#include <groonga.h>

struct PGrnBuffers
{
	grn_obj general;
	grn_obj path;
	grn_obj keyword;
	grn_obj pattern;
	grn_obj ctid;
	grn_obj score;
	grn_obj sourceIDs;
	grn_obj head;
	grn_obj body;
	grn_obj foot;
	grn_obj inspect;
};

grn_ctx PGrnContext;
struct PGrnBuffers PGrnBuffers;

void PGrnInitializeBuffers(void);
void PGrnFinalizeBuffers(void);
