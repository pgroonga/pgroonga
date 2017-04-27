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
	grn_obj tokenFilters;
	grn_obj jsonbValueKeys;
	grn_obj walPosition;
	grn_obj walValue;
	grn_obj maxRecordSize;
	grn_obj walAppliedPosition;
	struct
	{
		grn_obj escapedValue;
		grn_obj specialCharacters;
	} escape;
	grn_obj head;
	grn_obj body;
	grn_obj foot;
	grn_obj inspect;
};

extern grn_ctx PGrnContext;
extern struct PGrnBuffers PGrnBuffers;
extern int PGrnKeepNSearchResults;

void PGrnInitializeBuffers(void);
void PGrnFinalizeBuffers(void);
