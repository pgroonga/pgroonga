#pragma once

#include <postgres.h>

#include <groonga.h>

#define VARCHARARRAYOID 1015

void PGrnConvertFromData(Datum datum, Oid typeID, grn_obj *buffer);
