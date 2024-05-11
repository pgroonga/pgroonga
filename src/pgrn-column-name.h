#pragma once

#include "pgrn-check.h"

#include <mb/pg_wchar.h>

#define PGRN_COLUMN_NAME_ENCODED_CHARACTER_FORMAT "@%05x"
#define PGRN_COLUMN_NAME_ENCODED_CHARACTER_LENGTH 6

static inline bool
PGrnColumnNameIsUsableCharacterASCII(char character)
{
	return (character == '_' || ('0' <= character && character <= '9') ||
			('A' <= character && character <= 'Z') ||
			('a' <= character && character <= 'z'));
}

static inline void
PGrnColumnNameEncodeCharacterUTF8(const char *utf8Character, char *encodedName)
{
	pg_wchar codepoint;
	codepoint = utf8_to_unicode((const unsigned char *) utf8Character);
	snprintf(encodedName,
			 PGRN_COLUMN_NAME_ENCODED_CHARACTER_LENGTH + 1,
			 PGRN_COLUMN_NAME_ENCODED_CHARACTER_FORMAT,
			 codepoint);
}

static inline void
PGrnColumnNameCheckSize(size_t size, const char *tag)
{
	if (size >= GRN_TABLE_MAX_KEY_SIZE)
		PGrnCheckRC(GRN_INVALID_ARGUMENT,
					"%s too large encoded column name >= %d",
					tag,
					GRN_TABLE_MAX_KEY_SIZE);
}

static inline size_t
PGrnColumnNameEncodeUTF8WithSize(const char *name,
								 size_t nameSize,
								 char *encodedName)
{
	const char *tag = "[column-name][encode][utf8]";
	const char *current;
	const char *end;
	char *encodedCurrent;
	size_t encodedNameSize = 0;

	current = name;
	end = name + nameSize;
	encodedCurrent = encodedName;
	while (current < end)
	{
		int length;

		length = grn_charlen(ctx, current, end);
		if (length == -1)
		{
			PGrnCheckRC(GRN_INVALID_ARGUMENT,
						"%s invalid character: <%.*s|%.*s>",
						tag,
						(int) (current - name),
						name,
						(int) (end - current),
						current);
		}

		if (length == 1 && PGrnColumnNameIsUsableCharacterASCII(*current) &&
			!(*current == '_' && current == name))
		{
			PGrnColumnNameCheckSize(encodedNameSize + length + 1, tag);
			*encodedCurrent++ = *current;
			encodedNameSize++;
		}
		else
		{
			PGrnColumnNameCheckSize(
				encodedNameSize + PGRN_COLUMN_NAME_ENCODED_CHARACTER_LENGTH + 1,
				tag);
			PGrnColumnNameEncodeCharacterUTF8(current, encodedCurrent);
			encodedCurrent += PGRN_COLUMN_NAME_ENCODED_CHARACTER_LENGTH;
			encodedNameSize += PGRN_COLUMN_NAME_ENCODED_CHARACTER_LENGTH;
		}

		current += length;
	}

	*encodedCurrent = '\0';

	return encodedNameSize;
}

static inline size_t
PGrnColumnNameEncodeWithSize(const char *name,
							 size_t nameSize,
							 char *encodedName)
{
	const char *tag = "[column-name][encode]";
	const char *current;
	const char *end;
	char *encodedCurrent;
	size_t encodedNameSize = 0;

	if (GRN_CTX_GET_ENCODING(ctx) == GRN_ENC_UTF8)
		return PGrnColumnNameEncodeUTF8WithSize(name, nameSize, encodedName);

	current = name;
	end = name + nameSize;
	encodedCurrent = encodedName;
	while (current < end)
	{
		int length;

		length = grn_charlen(ctx, current, end);
		if (length != 1)
			PGrnCheckRC(GRN_FUNCTION_NOT_IMPLEMENTED,
						"%s multibyte character isn't supported "
						"for column name except UTF-8 encoding: <%s>(%s)",
						tag,
						name,
						grn_encoding_to_string(GRN_CTX_GET_ENCODING(ctx)));

		if (PGrnColumnNameIsUsableCharacterASCII(*current) &&
			!(*current == '_' && current == name))
		{
			PGrnColumnNameCheckSize(encodedNameSize + length + 1, tag);
			*encodedCurrent++ = *current;
			encodedNameSize++;
		}
		else
		{
			PGrnColumnNameCheckSize(
				encodedNameSize + PGRN_COLUMN_NAME_ENCODED_CHARACTER_LENGTH + 1,
				tag);
			PGrnColumnNameEncodeCharacterUTF8(current, encodedCurrent);
			encodedCurrent += PGRN_COLUMN_NAME_ENCODED_CHARACTER_LENGTH;
			encodedNameSize += PGRN_COLUMN_NAME_ENCODED_CHARACTER_LENGTH;
		}

		current++;
	}

	*encodedCurrent = '\0';

	return encodedNameSize;
}

static inline size_t
PGrnColumnNameEncode(const char *name, char *encodedName)
{
	return PGrnColumnNameEncodeWithSize(name, strlen(name), encodedName);
}

static inline size_t
PGrnColumnNameDecode(const char *encodedName, char *name)
{
	/* TODO */
	return 0;
}
