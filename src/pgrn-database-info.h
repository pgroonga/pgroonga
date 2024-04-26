#pragma once

#define PGRN_DATABASE_INFO_PACK(databaseOid, tableSpaceOid)                    \
	((((uint64) (databaseOid)) << (sizeof(Oid) * 8)) + (tableSpaceOid))
#define PGRN_DATABASE_INFO_UNPACK(info, databaseOid, tableSpaceOid)            \
	do                                                                         \
	{                                                                          \
		databaseOid = (info) >> (sizeof(Oid) * 8);                             \
		tableSpaceOid = (info) & ((((uint64) 1) << sizeof(Oid) * 8) - 1);      \
	} while (false)
