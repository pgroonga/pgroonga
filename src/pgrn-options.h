#pragma once

#include <groonga.h>

#include <postgres.h>
#include <utils/rel.h>

void PGrnInitializeOptions(void);

void PGrnApplyOptionValues(Relation index,
						   grn_obj **tokenizer,
						   const char *defaultTokenizerName,
						   grn_obj **normalizer,
						   const char *defaultNormalizerName,
						   grn_obj *tokenFilters);

bytea *pgroonga_options_raw(Datum reloptions,
							bool validate);
