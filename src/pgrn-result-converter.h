#pragma once

#include <postgres.h>

#include <access/htup_details.h>
#include <access/tupdesc.h>
#include <utils/jsonb.h>

#include "pgrn-groonga.h"

typedef struct
{
	const char *tag;
	JsonbIterator *iterator;
	grn_command_version commandVersion;
	TupleDesc desc;
} PGrnResultConverter;

void PGrnResultConverterInit(PGrnResultConverter *converter,
							 Jsonb *jsonb,
							 const char *tag);
void PGrnResultConverterBuildTupleDesc(PGrnResultConverter *converter);
HeapTuple PGrnResultConverterBuildTuple(PGrnResultConverter *converter);
Jsonb *PGrnResultConverterBuildJSONBObjects(PGrnResultConverter *converter);
