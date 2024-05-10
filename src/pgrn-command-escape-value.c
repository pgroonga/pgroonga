#include "pgroonga.h"

#include "pgrn-compatible.h"

#include "pgrn-command-escape-value.h"
#include "pgrn-global.h"

#include <utils/builtins.h>
#ifdef PGRN_HAVE_VARATT_H
#	include <varatt.h>
#endif

static struct PGrnBuffers *buffers = &PGrnBuffers;

PGDLLEXPORT PG_FUNCTION_INFO_V1(pgroonga_command_escape_value);

void
PGrnCommandEscapeValue(const char *value,
					   size_t valueSize,
					   grn_obj *escapedValue)
{
	const char *valueCurrent;
	const char *valueEnd;

	GRN_TEXT_PUTC(ctx, escapedValue, '"');
	valueCurrent = value;
	valueEnd = valueCurrent + valueSize;
	while (valueCurrent < valueEnd)
	{
		int charLength = grn_charlen(ctx, valueCurrent, valueEnd);

		if (charLength == 0)
		{
			break;
		}
		else if (charLength == 1)
		{
			switch (*valueCurrent)
			{
			case '\\':
			case '"':
				GRN_TEXT_PUTC(ctx, escapedValue, '\\');
				GRN_TEXT_PUTC(ctx, escapedValue, *valueCurrent);
				break;
			case '\n':
				GRN_TEXT_PUTS(ctx, escapedValue, "\\n");
				break;
			default:
				GRN_TEXT_PUTC(ctx, escapedValue, *valueCurrent);
				break;
			}
		}
		else
		{
			GRN_TEXT_PUT(ctx, escapedValue, valueCurrent, charLength);
		}

		valueCurrent += charLength;
	}
	GRN_TEXT_PUTC(ctx, escapedValue, '"');
}

/**
 * pgroonga.command_escape_value(value text) : text
 */
Datum
pgroonga_command_escape_value(PG_FUNCTION_ARGS)
{
	text *value = PG_GETARG_TEXT_PP(0);
	text *escapedValue;
	grn_obj *escapedValueBuffer;

	escapedValueBuffer = &(buffers->escape.escapedValue);
	GRN_BULK_REWIND(escapedValueBuffer);
	PGrnCommandEscapeValue(
		VARDATA_ANY(value), VARSIZE_ANY_EXHDR(value), escapedValueBuffer);
	escapedValue = cstring_to_text_with_len(GRN_TEXT_VALUE(escapedValueBuffer),
											GRN_TEXT_LEN(escapedValueBuffer));
	PG_RETURN_TEXT_P(escapedValue);
}
