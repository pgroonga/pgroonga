#pragma once

#include <sys/stat.h>
#include <sys/types.h>
#ifndef _WIN32
#	include <unistd.h>
#endif

#ifdef _WIN32
typedef struct _stat pgrn_stat_buffer;
#	define pgrn_stat(path, buffer) _stat(path, buffer)
#else
typedef struct stat pgrn_stat_buffer;
#	define pgrn_stat(path, buffer) stat(path, buffer)
#endif
