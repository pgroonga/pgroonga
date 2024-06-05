#pragma once

#include <groonga.h>

void PGrnStringSubstituteIndex(const char *text,
							   unsigned int textSize,
							   grn_obj *output,
							   const char *indexName,
							   int section);
void PGrnStringSubstituteVariables(const char *string,
								   unsigned int stringSize,
								   grn_obj *output);
bool PGrnRawStringIsEmpty(const char *string, unsigned int stringSize);
