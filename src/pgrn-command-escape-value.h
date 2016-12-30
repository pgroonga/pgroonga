#pragma once

#include <groonga.h>

void PGrnCommandEscapeValue(const char *value,
							size_t valueSize,
							grn_obj *escapedValue);
