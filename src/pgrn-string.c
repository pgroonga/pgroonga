#include "pgrn-global.h"
#include "pgrn-groonga.h"
#include "pgrn-string.h"

static grn_ctx *ctx = &PGrnContext;
static struct PGrnBuffers *buffers = &PGrnBuffers;

void
PGrnStringSubstituteIndex(const char *string,
						  unsigned int stringSize,
						  grn_obj *output,
						  const char *indexName,
						  int section)
{
	const char variable[] = "$index";
	const size_t variableSize = sizeof(variable) - 1;
	const char *current = string;
	const char *end = current + stringSize;

	while (current < end)
	{
		int char_length = grn_charlen(ctx, current, end);
		if (char_length == 0) {
			return;
		}
		if (char_length == 1 &&
			current[0] == '$' &&
			(end - current) >= variableSize &&
			memcmp(current, variable, variableSize) == 0)
		{
			grn_text_printf(ctx, output, "%s[%d]", indexName, section);
			current += variableSize;
			continue;
		}

		GRN_TEXT_PUT(ctx, output, current, char_length);
		current += char_length;
	}
}

void
PGrnStringSubstituteVariables(const char *string,
							  unsigned int stringSize,
							  grn_obj *output)
{
	const char *current = string;
	const char *end = current + stringSize;
	enum {
		STATE_RAW,
		STATE_ESCAPE,
		STATE_VARIABLE_START,
		STATE_IN_VARIABLE_BLOCK,
	} state = STATE_RAW;
	const char *variableSubstitutionStart = NULL;
	enum {
		VARIABLE_TYPE_NONE,
		VARIABLE_TYPE_TABLE,
		/* TODO */
		/* VARIABLE_TYPE_INDEX_COLUMN, */
	} variableType = VARIABLE_TYPE_NONE;
	const char *targetStart = NULL;

	while (current < end)
	{
		int charLength = grn_charlen(ctx, current, end);
		if (charLength == 0) {
			return;
		}
		switch (state)
		{
		case STATE_RAW:
			if (charLength == 1 && current[0] == '\\')
			{
				state = STATE_ESCAPE;
			}
			else if (charLength == 1 && current[0] == '$')
			{
				state = STATE_VARIABLE_START;
				variableSubstitutionStart = current;
			}
			else
			{
				GRN_TEXT_PUT(ctx, output, current, charLength);
			}
			break;
		case STATE_ESCAPE:
			GRN_TEXT_PUT(ctx, output, current, charLength);
			state = STATE_RAW;
			break;
		case STATE_VARIABLE_START:
			if (charLength == 1 && current[0] == '{')
			{
				state = STATE_IN_VARIABLE_BLOCK;
			}
			else
			{
				/* Not variable substitution. */
				GRN_TEXT_PUT(ctx,
							 output,
							 variableSubstitutionStart,
							 (current - variableSubstitutionStart) + charLength);
				state = STATE_RAW;
				variableSubstitutionStart = NULL;
				variableType = VARIABLE_TYPE_NONE;
				targetStart = NULL;
			}
			break;
		case STATE_IN_VARIABLE_BLOCK:
			if (charLength == 1 &&
				variableType == VARIABLE_TYPE_NONE &&
				current[0] == ':')
			{
				grn_raw_string rawVariable;
				rawVariable.value = variableSubstitutionStart + strlen("${");
				rawVariable.length = current - rawVariable.value;
				if (GRN_RAW_STRING_EQUAL_CSTRING(rawVariable, "table"))
				{
					variableType = VARIABLE_TYPE_TABLE;
					targetStart = current + charLength;
				}
				/*
				else if (GRN_RAW_STRING_EQUAL_CSTRING(rawVariable, "index_column"))
				{
					variableType = VARIABLE_TYPE_INDEX_COLUMN;
					targetStart = current + charLength;
				}
				*/
			}
			else if (charLength == 1 && current[0] == '}')
			{
				switch (variableType)
				{
				case VARIABLE_TYPE_TABLE:
				{
					grn_obj *indexName = &(buffers->text);
					char tableName[GRN_TABLE_MAX_KEY_SIZE];
					GRN_TEXT_SET(ctx,
								 indexName,
								 targetStart,
								 current - targetStart);
					GRN_TEXT_PUTC(ctx, indexName, '\0');
					PGrnFormatSourcesTableName(GRN_TEXT_VALUE(indexName),
											   tableName);
					GRN_TEXT_PUTS(ctx, output, tableName);
					break;
				}
				/* TODO:  */
				/* case VARIABLE_TYPE_INDEX_COLUMN: */
				default:
					/* Not variable substitution. */
					GRN_TEXT_PUT(ctx,
								 output,
								 variableSubstitutionStart,
								 (current - variableSubstitutionStart) + charLength);
					break;
				}
				state = STATE_RAW;
				variableSubstitutionStart = NULL;
				variableType = VARIABLE_TYPE_NONE;
				targetStart = NULL;
			}
			break;
		default:
			break;
		}
		current += charLength;
	}
}
