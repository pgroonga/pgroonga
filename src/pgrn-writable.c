#include "pgroonga.h"

#include "pgrn-compatible.h"
#include "pgrn-writable.h"

#define KEY "pgroonga_writable"
#define KEY_SIZE (sizeof(KEY) - 1)
#define FALSE_VALUE "false"
#define FALSE_VALUE_SIZE (sizeof(FALSE_VALUE) - 1)

PGDLLEXPORT PG_FUNCTION_INFO_V1(pgroonga_is_writable);
PGDLLEXPORT PG_FUNCTION_INFO_V1(pgroonga_set_writable);

bool
PGrnIsWritable(void)
{
	const char *value = NULL;
	uint32_t valueSize = 0;

	if (!PGrnGroongaInitialized)
	{
		return true;
	}

	grn_config_get(ctx, KEY, KEY_SIZE, &value, &valueSize);
	return valueSize == 0;
}

void
PGrnSetWritable(bool newWritable)
{
	if (!PGrnGroongaInitialized)
	{
		return;
	}

	if (newWritable)
		grn_config_delete(ctx, KEY, KEY_SIZE);
	else
		grn_config_set(ctx, KEY, KEY_SIZE, FALSE_VALUE, FALSE_VALUE_SIZE);
}

/**
 * pgroonga_is_writable() : bool
 */
Datum
pgroonga_is_writable(PG_FUNCTION_ARGS)
{
	const bool isWritable = PGrnIsWritable();
	PG_RETURN_BOOL(isWritable);
}

/**
 * pgroonga_set_writable(bool writable) : bool
 */
Datum
pgroonga_set_writable(PG_FUNCTION_ARGS)
{
	const bool newWritable = PG_GETARG_BOOL(0);
	const bool currentWritable = PGrnIsWritable();

	PGrnSetWritable(newWritable);

	PG_RETURN_BOOL(currentWritable);
}
