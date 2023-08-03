#pragma once

#include "pgrn-compatible.h"
#include "pgrn-database-info.h"

#include <c.h>
#ifdef PGRN_HAVE_COMMON_HASHFN_H
#	include <common/hashfn.h>
#endif
#include <miscadmin.h>
#include <port/atomics.h>
#include <storage/shmem.h>

#include <signal.h>

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
	PGRN_DATABASE_INFO_UNPACK(*((const uint64 *)key),
							  databaseOid,
							  tableSpaceOid);
#ifdef PGRN_HAVE_COMMON_HASHFN_H
	return hash_combine(uint32_hash(&databaseOid, sizeof(Oid)),
						uint32_hash(&tableSpaceOid, sizeof(Oid)));
#else
	return databaseOid ^ tableSpaceOid;
#endif
}

static inline HTAB *
pgrn_crash_safer_statuses_get(void)
{
	const char *name = "pgrn-crash-safer-statuses";
	HASHCTL info;
	int flags;
	info.keysize = sizeof(uint64);
	info.entrysize = sizeof(pgrn_crash_safer_statuses_entry);
	info.hash = pgrn_crash_safer_statuses_hash;
	flags = HASH_ELEM | HASH_FUNCTION;
	return ShmemInitHash(name,
						 1,
						 32 /* TODO: configurable */,
						 &info,
						 flags);
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
	if (!statuses) {
		statuses = pgrn_crash_safer_statuses_get();
	}
	databaseInfo = PGRN_DATABASE_INFO_PACK(databaseOid, tableSpaceOid);
	entry = hash_search(statuses, &databaseInfo, action, &found_local);
	if (action == HASH_ENTER && !found_local) {
		entry->pid = InvalidPid;
		entry->preparePID = InvalidPid;
	}
	if (found) {
		*found = found_local;
	}
	return entry;
}

static inline void
pgrn_crash_safer_statuses_set_main_pid(HTAB *statuses, pid_t pid)
{
	pgrn_crash_safer_statuses_entry *entry;
	entry = pgrn_crash_safer_statuses_search(statuses,
											 InvalidOid,
											 InvalidOid,
											 HASH_ENTER,
											 NULL);
	entry->pid = pid;
}

static inline pid_t
pgrn_crash_safer_statuses_get_main_pid(HTAB *statuses)
{
	bool found;
	pgrn_crash_safer_statuses_entry *entry;
	entry = pgrn_crash_safer_statuses_search(statuses,
											 InvalidOid,
											 InvalidOid,
											 HASH_FIND,
											 &found);
	if (found) {
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
	entry = pgrn_crash_safer_statuses_search(statuses,
											 databaseOid,
											 tableSpaceOid,
											 HASH_ENTER,
											 NULL);
	entry->preparePID = pid;
}

static inline pid_t
pgrn_crash_safer_statuses_get_prepare_pid(HTAB *statuses,
										  Oid databaseOid,
										  Oid tableSpaceOid)
{
	bool found;
	pgrn_crash_safer_statuses_entry *entry;
	entry = pgrn_crash_safer_statuses_search(statuses,
											 databaseOid,
											 tableSpaceOid,
											 HASH_FIND,
											 &found);
	if (found) {
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
	entry = pgrn_crash_safer_statuses_search(statuses,
											 databaseOid,
											 tableSpaceOid,
											 HASH_ENTER,
											 NULL);
	pg_atomic_fetch_add_u32(&(entry->nUsingProcesses), 1);
}

static inline void
pgrn_crash_safer_statuses_release(HTAB *statuses,
								  Oid databaseOid,
								  Oid tableSpaceOid)
{
	bool found;
	pgrn_crash_safer_statuses_entry *entry;
	entry = pgrn_crash_safer_statuses_search(statuses,
											 databaseOid,
											 tableSpaceOid,
											 HASH_FIND,
											 &found);
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
	entry = pgrn_crash_safer_statuses_search(statuses,
											 databaseOid,
											 tableSpaceOid,
											 HASH_FIND,
											 &found);
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
	entry = pgrn_crash_safer_statuses_search(statuses,
											 databaseOid,
											 tableSpaceOid,
											 HASH_ENTER,
											 NULL);
	entry->flushing = true;
}

static inline void
pgrn_crash_safer_statuses_stop(HTAB *statuses,
							   Oid databaseOid,
							   Oid tableSpaceOid)
{
	pgrn_crash_safer_statuses_search(statuses,
									 databaseOid,
									 tableSpaceOid,
									 HASH_REMOVE,
									 NULL);
}

static inline bool
pgrn_crash_safer_statuses_is_flushing(HTAB *statuses,
									  Oid databaseOid,
									  Oid tableSpaceOid)
{
	bool found;
	pgrn_crash_safer_statuses_entry *entry;
	entry = pgrn_crash_safer_statuses_search(statuses,
											 databaseOid,
											 tableSpaceOid,
											 HASH_FIND,
											 &found);
	return found && entry->flushing;
}

static inline bool
pgrn_crash_safer_statuses_is_preparing(HTAB *statuses,
									   Oid databaseOid,
									   Oid tableSpaceOid)
{
	bool found;
	pgrn_crash_safer_statuses_entry *entry;
	entry = pgrn_crash_safer_statuses_search(statuses,
											 databaseOid,
											 tableSpaceOid,
											 HASH_FIND,
											 &found);
	return found && entry->preparePID != InvalidPid;
}
