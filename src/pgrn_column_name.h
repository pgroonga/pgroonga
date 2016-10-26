#pragma once

size_t PGrnColumnNameEncode(const char *name, char *encodedName);
size_t PGrnColumnNameEncodeWithSize(const char *name,
									size_t nameSize,
									char *encodedName);
size_t PGrnColumnNameDecode(const char *encodedName, char *name);
