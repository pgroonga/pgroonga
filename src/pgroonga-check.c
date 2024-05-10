#include "pgrn-database.h"

#include <postgres.h>

#include <fmgr.h>
#include <miscadmin.h>

#include <groonga.h>

PG_MODULE_MAGIC;

extern PGDLLEXPORT void _PG_init(void);

static uint32_t
PGrnGetThreadLimit(void *data)
{
	return 1;
}

static void
PGrnCheckDatabaseDirectory(grn_ctx *ctx, const char *directoryPath)
{
	char databasePath[MAXPGPATH];
	grn_obj *db;
	pgrn_stat_buffer fileStatus;

	join_path_components(databasePath, directoryPath, PGrnDatabaseBasename);

	if (pgrn_stat(databasePath, &fileStatus) != 0)
	{
		return;
	}

	db = grn_db_open(ctx, databasePath);
	if (!db)
	{
		PGrnDatabaseRemoveAllRelatedFiles(directoryPath);
		return;
	}

	grn_db_recover(ctx, db);
	if (ctx->rc != GRN_SUCCESS)
	{
		grn_obj_remove(ctx, db);
		PGrnDatabaseRemoveAllRelatedFiles(directoryPath);
		return;
	}

	grn_obj_close(ctx, db);
}

static void
PGrnCheckAllDatabases(grn_ctx *ctx)
{
	const char *baseDirectoryPath = "base";

#ifdef _WIN32
	WIN32_FIND_DATA data;
	HANDLE finder;
	char targetPath[MAXPGPATH];

	join_path_components(targetPath, baseDirectoryPath, "*");
	finder = FindFirstFile(targetPath, &data);
	if (finder != INVALID_HANDLE_VALUE)
	{
		do
		{
			char directoryPath[MAXPGPATH];

			if (strcmp(data.cFileName, ".") == 0)
				continue;
			if (strcmp(data.cFileName, "..") == 0)
				continue;
			if (!(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
				continue;

			join_path_components(
				directoryPath, baseDirectoryPath, data.cFileName);
			PGrnCheckDatabaseDirectory(ctx, directoryPath);
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
			char directoryPath[MAXPGPATH];

			if (strcmp(entry->d_name, ".") == 0)
				continue;
			if (strcmp(entry->d_name, "..") == 0)
				continue;

			join_path_components(
				directoryPath, baseDirectoryPath, entry->d_name);
			PGrnCheckDatabaseDirectory(ctx, directoryPath);
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
				 errmsg("pgroonga: check: failed to initialize Groonga")));

	if (grn_ctx_init(ctx, 0) != GRN_SUCCESS)
		ereport(ERROR,
				(errcode(ERRCODE_SYSTEM_ERROR),
				 errmsg("pgroonga: check: "
						"failed to initialize Groonga context")));

	GRN_LOG(
		ctx, GRN_LOG_NOTICE, "pgroonga: check: initialize: <%s>", PGRN_VERSION);

	PGrnCheckAllDatabases(ctx);

	grn_ctx_fin(ctx);

	grn_fin();
}
