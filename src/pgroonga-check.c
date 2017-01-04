#include "pgroonga.h"

#include "pgrn-portable.h"

#include <groonga.h>

#include <sys/types.h>
#include <sys/stat.h>
#ifndef WIN32
#	include <dirent.h>
#	include <unistd.h>
#endif

PG_MODULE_MAGIC;

extern PGDLLEXPORT void _PG_init(void);

static uint32_t
PGrnGetThreadLimit(void *data)
{
	return 1;
}

static void
PGrnRemoveAllRelatedFiles(const char *databaseDirectoryPath)
{
#ifdef WIN32
	WIN32_FIND_DATA data;
	HANDLE finder = FindFirstFile(databaseDirectoryPath, &data);
	if (finder != INVALID_HANDLE_VALUE)
	{
		do
		{
			char targetPathPrefix[MAXPGPATH];
			join_path_components(targetPathPrefix,
								 databaseDirectoryPath,
								 PGrnDatabaseBasename);
			if (strncmp(data.cFileName,
						targetPathPrefix,
						strlen(targetPathPrefix)) == 0)
			{
				unlink(data.cFileName);
			}
		} while (FindNextFile(finder, &data) != 0);
		FindClose(finder);
	}
#else
	DIR *dir = opendir(databaseDirectoryPath);
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
				join_path_components(path,
									 databaseDirectoryPath,
									 entry->d_name);
				unlink(path);
			}
		}
		closedir(dir);
	}
#endif
}

static void
PGrnCheckDatabaseDirectory(grn_ctx *ctx, const char *databaseDirectoryPath)
{
	char databasePath[MAXPGPATH];
	grn_obj	*db;
	pgrn_stat_buffer fileStatus;

	join_path_components(databasePath,
						 databaseDirectoryPath,
						 PGrnDatabaseBasename);

	if (pgrn_stat(databasePath, &fileStatus) != 0)
	{
		return;
	}

	db = grn_db_open(ctx, databasePath);
	if (!db)
	{
		PGrnRemoveAllRelatedFiles(databaseDirectoryPath);
		return;
	}

	grn_obj_close(ctx, db);
}

static void
PGrnCheck(grn_ctx *ctx)
{
	/* TODO: Support table space: "pg_tblspc/" */
	const char *baseDirectoryPath = "base";

#ifdef WIN32
	WIN32_FIND_DATA data;
	HANDLE finder = FindFirstFile(baseDirectoryPath, &data);
	if (finder != INVALID_HANDLE_VALUE)
	{
		do
		{
			if (!(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			{
				continue;
			}
			PGrnCheckDatabaseDirectory(ctx, data.cFileName);
		} while (FindNextFile(finder, &data) != 0);
		FindClose(finder);
	}
#else
	DIR *dir = opendir(baseDirectoryPath);
	if (dir)
	{
		struct dirent *entry;
		while ((entry = readdir(dir)))
		{
			char databaseDirectoryPath[MAXPGPATH];

			if (strcmp(entry->d_name, ".") == 0)
				continue;
			if (strcmp(entry->d_name, "..") == 0)
				continue;

			join_path_components(databaseDirectoryPath,
								 baseDirectoryPath,
								 entry->d_name);
			PGrnCheckDatabaseDirectory(ctx, databaseDirectoryPath);
		}
		closedir(dir);
	}
#endif
}

void
_PG_init(void)
{
	grn_ctx ctx_;
	grn_ctx *ctx = &ctx_;

	if (IsUnderPostmaster)
		return;

	grn_thread_set_get_limit_func(PGrnGetThreadLimit, NULL);

	grn_default_logger_set_flags(grn_default_logger_get_flags() | GRN_LOG_PID);

	if (grn_init() != GRN_SUCCESS)
		ereport(ERROR,
				(errcode(ERRCODE_SYSTEM_ERROR),
				 errmsg("pgroonga: failed to initialize Groonga")));

	if (grn_ctx_init(ctx, 0))
		ereport(ERROR,
				(errcode(ERRCODE_SYSTEM_ERROR),
				 errmsg("pgroonga: failed to initialize Groonga context")));

	GRN_LOG(ctx, GRN_LOG_NOTICE, "pgroonga: initialize: <%s>", PGRN_VERSION);

	PGrnCheck(ctx);

	grn_ctx_fin(ctx);

	grn_fin();
}
