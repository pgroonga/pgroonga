#pragma once

#include "pgrn-compatible.h"
#include "pgrn-database-info.h"

#include <c.h>
#include <common/hashfn.h>
#include <miscadmin.h>
#include <port/atomics.h>
#include <storage/shmem.h>

#include <signal.h>

/* The version of pgrn_crash_safer_statuses_entry layout.
 *
 * PostgreSQL isn't restarted on upgrading PGroonga in compatible
 * case. So new pgroonga.so may be used with old pgroonga_crash_safer
 * that created statuses with old layout. We must keep the following
 * rules to keep compatibility:
 *
 *   * Don't change existing members.
 *   * Add a new member only to the last.
 *   * Increment this when a new member is added.
 *   * Don't touch a new member when
 *     pgrn_crash_safer_statuses_get_version() is older than the
 *     version that added the new member.
 *
 * History:
 *
 *   * 0: No version information.
 *   * 1: Added version information.
 */
#define PGRN_CRASH_SAFER_STATUSES_VERSION 1

typedef struct pgrn_crash_safer_statuses_entry
{
	uint64 key;
	pid_t pid;
	pid_t preparePID;
	sig_atomic_t flushing;
	pg_atomic_uint32 nUsingProcesses;
} pgrn_crash_safer_statuses_entry;

static inline uint32
pgrn_crash_safer_statuses_hash(const void *key, Size keysize)
{
	Oid databaseOid;
	Oid tableSpaceOid;
	PGRN_DATABASE_INFO_UNPACK(
		*((const uint64 *) key), databaseOid, tableSpaceOid);
	return hash_combine(uint32_hash(&databaseOid, sizeof(Oid)),
						uint32_hash(&tableSpaceOid, sizeof(Oid)));
}

static inline HTAB *
pgrn_crash_safer_statuses_get(void)
{
	const char *name = "pgrn-crash-safer-statuses";
	HASHCTL info;
	int flags;
	memset(&info, 0, sizeof(info));
	info.keysize = sizeof(uint64);
	info.entrysize = sizeof(pgrn_crash_safer_statuses_entry);
	info.hash = pgrn_crash_safer_statuses_hash;
	flags = HASH_ELEM | HASH_FUNCTION;
	return PGrnShmemInitHash(
		name, 1, 32 /* TODO: configurable */, &info, flags);
}

static inline pgrn_crash_safer_statuses_entry *
pgrn_crash_safer_statuses_search(HTAB *statuses,
								 Oid databaseOid,
								 Oid tableSpaceOid,
								 HASHACTION action,
								 bool *found)
{
	uint64 databaseInfo;
	bool found_local;
	pgrn_crash_safer_statuses_entry *entry;
	if (!statuses)
	{
		statuses = pgrn_crash_safer_statuses_get();
	}
	databaseInfo = PGRN_DATABASE_INFO_PACK(databaseOid, tableSpaceOid);
	entry = hash_search(statuses, &databaseInfo, action, &found_local);
	if (action == HASH_ENTER && !found_local)
	{
		entry->pid = InvalidPid;
		entry->preparePID = InvalidPid;
	}
	if (found)
	{
		*found = found_local;
	}
	return entry;
}

static inline int *
pgrn_crash_safer_statuses_get_version_address(void)
{
	bool found;
	int *version = ShmemInitStruct(
		"pgrn-crash-safer-statuses-version", sizeof(int), &found);
	/* pgroonga_crash_safer that doesn't have version information
	 * doesn't create this. */
	if (!found)
		*version = 0;
	return version;
}

/* pgroonga_crash_safer calls this when shared memory is
 * initialized. */
static inline void
pgrn_crash_safer_statuses_register_version(void)
{
	*pgrn_crash_safer_statuses_get_version_address() =
		PGRN_CRASH_SAFER_STATUSES_VERSION;
}

/* This returns the layout version of statuses. statuses are created
 * by pgroonga_crash_safer. So this may be older than
 * PGRN_CRASH_SAFER_STATUSES_VERSION when pgroonga_crash_safer is
 * older than the caller. */
static inline int
pgrn_crash_safer_statuses_get_version(void)
{
	return *pgrn_crash_safer_statuses_get_version_address();
}

/* pgroonga_crash_safer calls this when shared memory is initialized.
 * The entry for the main process exists but its PID is InvalidPid
 * until the main process is started. */
static inline void
pgrn_crash_safer_statuses_register_main(HTAB *statuses)
{
	pgrn_crash_safer_statuses_search(
		statuses, InvalidOid, InvalidOid, HASH_ENTER, NULL);
}

/* This returns true when the main process is started or will be
 * started. */
static inline bool
pgrn_crash_safer_statuses_has_main(HTAB *statuses)
{
	bool found;
	pgrn_crash_safer_statuses_search(
		statuses, InvalidOid, InvalidOid, HASH_FIND, &found);
	return found;
}

static inline void
pgrn_crash_safer_statuses_set_main_pid(HTAB *statuses, pid_t pid)
{
	pgrn_crash_safer_statuses_entry *entry;
	entry = pgrn_crash_safer_statuses_search(
		statuses, InvalidOid, InvalidOid, HASH_ENTER, NULL);
	entry->pid = pid;
}

static inline pid_t
pgrn_crash_safer_statuses_get_main_pid(HTAB *statuses)
{
	bool found;
	pgrn_crash_safer_statuses_entry *entry;
	entry = pgrn_crash_safer_statuses_search(
		statuses, InvalidOid, InvalidOid, HASH_FIND, &found);
	if (found)
	{
		return entry->pid;
	}
	else
	{
		return InvalidPid;
	}
}

static inline void
pgrn_crash_safer_statuses_set_prepare_pid(HTAB *statuses,
										  Oid databaseOid,
										  Oid tableSpaceOid,
										  pid_t pid)
{
	pgrn_crash_safer_statuses_entry *entry;
	entry = pgrn_crash_safer_statuses_search(
		statuses, databaseOid, tableSpaceOid, HASH_ENTER, NULL);
	entry->preparePID = pid;
}

static inline pid_t
pgrn_crash_safer_statuses_get_prepare_pid(HTAB *statuses,
										  Oid databaseOid,
										  Oid tableSpaceOid)
{
	bool found;
	pgrn_crash_safer_statuses_entry *entry;
	entry = pgrn_crash_safer_statuses_search(
		statuses, databaseOid, tableSpaceOid, HASH_FIND, &found);
	if (found)
	{
		return entry->preparePID;
	}
	else
	{
		return InvalidPid;
	}
}

static inline void
pgrn_crash_safer_statuses_use(HTAB *statuses,
							  Oid databaseOid,
							  Oid tableSpaceOid)
{
	pgrn_crash_safer_statuses_entry *entry;
	entry = pgrn_crash_safer_statuses_search(
		statuses, databaseOid, tableSpaceOid, HASH_ENTER, NULL);
	pg_atomic_fetch_add_u32(&(entry->nUsingProcesses), 1);
}

static inline void
pgrn_crash_safer_statuses_release(HTAB *statuses,
								  Oid databaseOid,
								  Oid tableSpaceOid)
{
	bool found;
	pgrn_crash_safer_statuses_entry *entry;
	entry = pgrn_crash_safer_statuses_search(
		statuses, databaseOid, tableSpaceOid, HASH_FIND, &found);
	if (found)
	{
		uint32 nUsingProcesses =
			pg_atomic_fetch_sub_u32(&(entry->nUsingProcesses), 1);
		if (nUsingProcesses == 1 && entry->pid != InvalidPid)
		{
			kill(entry->pid, SIGUSR1);
		}
	}
}

static inline uint32
pgrn_crash_safer_statuses_get_n_using_processes(HTAB *statuses,
												Oid databaseOid,
												Oid tableSpaceOid)
{
	bool found;
	pgrn_crash_safer_statuses_entry *entry;
	entry = pgrn_crash_safer_statuses_search(
		statuses, databaseOid, tableSpaceOid, HASH_FIND, &found);
	if (!found)
		return 0;
	return pg_atomic_read_u32(&(entry->nUsingProcesses));
}

static inline void
pgrn_crash_safer_statuses_start(HTAB *statuses,
								Oid databaseOid,
								Oid tableSpaceOid)
{
	pgrn_crash_safer_statuses_entry *entry;
	entry = pgrn_crash_safer_statuses_search(
		statuses, databaseOid, tableSpaceOid, HASH_ENTER, NULL);
	entry->flushing = true;
}

static inline void
pgrn_crash_safer_statuses_stop(HTAB *statuses,
							   Oid databaseOid,
							   Oid tableSpaceOid)
{
	pgrn_crash_safer_statuses_search(
		statuses, databaseOid, tableSpaceOid, HASH_REMOVE, NULL);
}

static inline bool
pgrn_crash_safer_statuses_is_flushing(HTAB *statuses,
									  Oid databaseOid,
									  Oid tableSpaceOid)
{
	bool found;
	pgrn_crash_safer_statuses_entry *entry;
	entry = pgrn_crash_safer_statuses_search(
		statuses, databaseOid, tableSpaceOid, HASH_FIND, &found);
	return found && entry->flushing;
}

static inline bool
pgrn_crash_safer_statuses_is_preparing(HTAB *statuses,
									   Oid databaseOid,
									   Oid tableSpaceOid)
{
	bool found;
	pgrn_crash_safer_statuses_entry *entry;
	entry = pgrn_crash_safer_statuses_search(
		statuses, databaseOid, tableSpaceOid, HASH_FIND, &found);
	return found && entry->preparePID != InvalidPid;
}
