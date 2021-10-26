#pragma once

#include "pgrn-portable.h"

#include <sys/types.h>
#include <sys/stat.h>
#ifndef WIN32
#	include <unistd.h>
#endif

static inline bool
pgrn_file_exist(const char *path)
{
	pgrn_stat_buffer fileStatus;
	return (pgrn_stat(path, &fileStatus) == 0);
}
