#include "pgrn_value.h"

#include <string.h>

#define PGRN_NONE_VALUE "none"

bool
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
