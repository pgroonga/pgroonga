#pragma once

#include <groonga.h>

struct PGrnBuffers {
	grn_obj inspect;
};

grn_ctx PGrnContext;
struct PGrnBuffers PGrnBuffers;
