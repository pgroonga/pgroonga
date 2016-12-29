#pragma once

#ifdef WIN32
typedef struct _stat pgrn_stat_buffer;
#	define pgrn_stat(path, buffer) _stat(path, buffer)
#else
typedef struct stat pgrn_stat_buffer;
#	define pgrn_stat(path, buffer) stat(path, buffer)
#endif
