#include "pgroonga.h"

#include "pgrn_global.h"
#include "pgrn_groonga.h"

#include <utils/builtins.h>

static grn_ctx *ctx = &PGrnContext;
static struct PGrnBuffers *buffers = &PGrnBuffers;

PG_FUNCTION_INFO_V1(pgroonga_command_escape_value);

/**
 * pgroonga.command_escape_value(value text : text
 */
Datum
pgroonga_command_escape_value(PG_FUNCTION_ARGS)
{
	text *value = PG_GETARG_TEXT_PP(0);
	text *escapedValue;
	grn_obj *escapedValueBuffer;
	const char *valueCurrent;
	const char *valueEnd;

	escapedValueBuffer = &(buffers->escape.escapedValue);

	GRN_BULK_REWIND(escapedValueBuffer);
	GRN_TEXT_PUTC(ctx, escapedValueBuffer, '"');
	valueCurrent = VARDATA_ANY(value);
	valueEnd = valueCurrent + VARSIZE_ANY_EXHDR(value);
	while (valueCurrent < valueEnd)
	{
		int charLength = grn_charlen(ctx, valueCurrent, valueEnd);

		if (charLength == 0) {
			break;
		}
		else if (charLength == 1)
		{
			switch (*valueCurrent)
			{
			case '\\':
			case '"':
				GRN_TEXT_PUTC(ctx, escapedValueBuffer, '\\');
				GRN_TEXT_PUTC(ctx, escapedValueBuffer, *valueCurrent);
				break;
			case '\n':
				GRN_TEXT_PUTS(ctx, escapedValueBuffer, "\\n");
				break;
			default:
				GRN_TEXT_PUTC(ctx, escapedValueBuffer, *valueCurrent);
				break;
			}
		}
		else
		{
			GRN_TEXT_PUT(ctx, escapedValueBuffer, valueCurrent, charLength);
		}

		valueCurrent += charLength;
	}
	GRN_TEXT_PUTC(ctx, escapedValueBuffer, '"');

	escapedValue = cstring_to_text_with_len(GRN_TEXT_VALUE(escapedValueBuffer),
											GRN_TEXT_LEN(escapedValueBuffer));
	PG_RETURN_TEXT_P(escapedValue);
}
