#pragma once

#include <postgres.h>

#include <string.h>

#define PGRN_NONE_VALUE "none"

static inline bool
PGrnIsNoneValue(const char *value)
{
	if (!value)
		return true;

	if (!value[0])
		return true;

	if (strcmp(value, PGRN_NONE_VALUE) == 0)
		return true;

	return false;
}

static inline bool
PGrnIsExplicitNoneValue(const char *value)
{
	if (!value)
		return false;

	if (!value[0])
		return true;

	if (strcmp(value, PGRN_NONE_VALUE) == 0)
		return true;

	return false;
}
