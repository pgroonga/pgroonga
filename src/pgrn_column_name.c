#include "pgroonga.h"

#include "pgrn_column_name.h"

#include <groonga.h>

#include <postgres.h>
#include <mb/pg_wchar.h>

static const char *ENCODED_CHARACTER_FORMAT = "@%05x";
static const int ENCODED_CHARACTER_LENGTH = 5;

static bool
PGrnColumnNameIsUsableCharacterASCII(char character)
{
	return (character == '_' ||
			('0' <= character && character <= '9') ||
			('A' <= character && character <= 'Z') ||
			('a' <= character && character <= 'z'));
}

static void
PGrnColumnNameEncodeCharacterUTF8(const char *utf8Character, char *encodedName)
{
	pg_wchar codepoint;
	codepoint = utf8_to_unicode((const unsigned char *)utf8Character);
	snprintf(encodedName,
			 ENCODED_CHARACTER_LENGTH + 1,
			 ENCODED_CHARACTER_FORMAT,
			 codepoint);
}

static void
checkSize(size_t size)
{
	if (size >= GRN_TABLE_MAX_KEY_SIZE)
		ereport(ERROR,
				(errcode(ERRCODE_NAME_TOO_LONG),
				 errmsg("pgroonga: encoded column name >= %d",
						GRN_TABLE_MAX_KEY_SIZE)));
}

static size_t
PGrnColumnNameEncodeUTF8(const char *name, char *encodedName)
{
	const char *current;
	char *encodedCurrent;
	size_t encodedNameSize = 0;

	current = name;
	encodedCurrent = encodedName;
	while (*current != '\0')
	{
		int length;

		length = pg_mblen(current);

		if (length == 1 &&
			PGrnColumnNameIsUsableCharacterASCII(*current) &&
			!(*current == '_' && current == name))
		{
			checkSize(encodedNameSize + length + 1);
			*encodedCurrent++ = *current;
			encodedNameSize++;
		}
		else
		{
			checkSize(encodedNameSize + ENCODED_CHARACTER_LENGTH + 1);
			PGrnColumnNameEncodeCharacterUTF8(current, encodedCurrent);
			encodedCurrent += ENCODED_CHARACTER_LENGTH;
			encodedNameSize += ENCODED_CHARACTER_LENGTH;
		}

		current += length;
	}

	*encodedCurrent = '\0';

	return encodedNameSize;
}

size_t
PGrnColumnNameEncode(const char *name, char *encodedName)
{
	const char *current;
	char *encodedCurrent;
	size_t encodedNameSize = 0;

	if (GetDatabaseEncoding() == PG_UTF8)
		return PGrnColumnNameEncodeUTF8(name, encodedName);

	current = name;
	encodedCurrent = encodedName;
	while (*current != '\0')
	{
		int length;

		length = pg_mblen(current);
		if (length != 1)
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("pgroonga: multibyte character isn't supported "
							"for column name except UTF-8 encoding: <%s>(%s)",
							name,
							GetDatabaseEncodingName())));

		if (PGrnColumnNameIsUsableCharacterASCII(*current) &&
			!(*current == '_' && current == name))
		{
			checkSize(encodedNameSize + length + 1);
			*encodedCurrent++ = *current;
			encodedNameSize++;
		}
		else
		{
			checkSize(encodedNameSize + ENCODED_CHARACTER_LENGTH + 1);
			PGrnColumnNameEncodeCharacterUTF8(current, encodedCurrent);
			encodedCurrent += ENCODED_CHARACTER_LENGTH;
			encodedNameSize += ENCODED_CHARACTER_LENGTH;
		}

		current++;
	}

	*encodedCurrent = '\0';

	return encodedNameSize;
}

size_t
PGrnColumnNameDecode(const char *encodedName, char *name)
{
	/* TODO */
	return 0;
}
