#include "pgroonga.h"

#include "pgrn-portable.h"

#include <sys/stat.h>
#include <sys/types.h>
#ifndef _WIN32
#	include <dirent.h>
#	include <unistd.h>
#endif

static void
PGrnDatabaseRemoveAllRelatedFiles(const char *directoryPath)
{
#ifdef _WIN32
	WIN32_FIND_DATA data;
	HANDLE finder;
	char targetPath[MAXPGPATH];

	join_path_components(targetPath, directoryPath, PGrnDatabaseBasename "*");
	finder = FindFirstFile(targetPath, &data);
	if (finder != INVALID_HANDLE_VALUE)
	{
		do
		{
			char path[MAXPGPATH];
			join_path_components(path, directoryPath, data.cFileName);
			unlink(path);
		} while (FindNextFile(finder, &data) != 0);
		FindClose(finder);
	}
#else
	DIR *dir = opendir(directoryPath);
	if (dir)
	{
		struct dirent *entry;
		while ((entry = readdir(dir)))
		{
			if (strncmp(entry->d_name,
						PGrnDatabaseBasename,
						strlen(PGrnDatabaseBasename)) == 0)
			{
				char path[MAXPGPATH];
				join_path_components(path, directoryPath, entry->d_name);
				unlink(path);
			}
		}
		closedir(dir);
	}
#endif
}
