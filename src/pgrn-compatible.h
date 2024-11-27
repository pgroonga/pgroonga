#pragma once

#include <postgres.h>

#ifdef _WIN32
#	define PRId64 "I64d"
#	define PRIu64 "I64u"
#	define PGRN_PRIdSIZE "Id"
#	define PGRN_PRIuSIZE "Iu"
#else
#	include <inttypes.h>
#	define PGRN_PRIdSIZE "zd"
#	define PGRN_PRIuSIZE "zu"
#endif

#ifdef PGRN_HAVE_MSGPACK
#	define PGRN_SUPPORT_WAL
#endif

#if PG_VERSION_NUM >= 150000
#	define PGRN_SUPPORT_WAL_RESOURCE_MANAGER
#endif

#if PG_VERSION_NUM >= 160000
#	define PGRN_HAVE_VARATT_H
#endif

#if PG_VERSION_NUM >= 140000
#	define PGRN_AM_INSERT_HAVE_INDEX_UNCHANGED
#endif

#if PG_VERSION_NUM >= 130000
#	define PGRN_INDEX_AM_ROUTINE_HAVE_AM_PARALLEL_VACUUM_OPTIONS
#endif

#if PG_VERSION_NUM >= 160000
#	define PGRN_RELATION_GET_LOCATOR(relation) ((relation)->rd_locator)
#	define PGRN_RELATION_GET_LOCATOR_NUMBER(relation)                         \
		((relation)->rd_locator.relNumber)
#	define PGRN_RELATION_GET_LOCATOR_SPACE(relation)                          \
		((relation)->rd_locator.spcOid)
#	include <common/relpath.h>
typedef RelFileNumber PGrnRelFileNumber;
#	define PGrnRelidByRelfilenumber(tablespaceOid, fileNumber)                \
		RelidByRelfilenumber((tablespaceOid), (fileNumber))
#	define pgrn_pg_tablespace_ownercheck(tablespaceOid, userOid)              \
		object_ownercheck(TableSpaceRelationId, (tablespaceOid), (userOid))
#	define pgrn_pg_class_ownercheck(relationOid, userOid)                     \
		object_ownercheck(RelationRelationId, (relationOid), (userOid))
#else
#	define PGRN_RELATION_GET_LOCATOR(relation) ((relation)->rd_node)
#	define PGRN_RELATION_GET_LOCATOR_NUMBER(relation)                         \
		((relation)->rd_node.relNode)
#	define PGRN_RELATION_GET_LOCATOR_SPACE(relation)                          \
		((relation)->rd_node.spcNode)
typedef Oid PGrnRelFileNumber;
#	define PGrnRelidByRelfilenumber(tablespaceOid, fileNumber)                \
		RelidByRelfilenode((tablespaceOid), (fileNumber))
#	define pgrn_pg_tablespace_ownercheck(tablespaceOid, userOid)              \
		pg_tablespace_ownercheck((tablespaceOid), (userOid))
#	define pgrn_pg_class_ownercheck(relationOid, userOid)                     \
		pg_class_ownercheck((relationOid), (userOid))
#endif

#if PG_VERSION_NUM >= 150000
#	define PGRN_RELKIND_HAS_PARTITIONS(relkind) RELKIND_HAS_PARTITIONS(relkind)
#	define PGRN_RELKIND_HAS_TABLE_AM(relkind) RELKIND_HAS_TABLE_AM(relkind)
#else
#	define PGRN_RELKIND_HAS_PARTITIONS(relkind)                               \
		((relkind) == RELKIND_PARTITIONED_TABLE ||                             \
		 (relkind) == RELKIND_PARTITIONED_INDEX)
#	define PGRN_RELKIND_HAS_TABLE_AM(relkind)                                 \
		((relkind) == RELKIND_RELATION || (relkind) == RELKIND_TOASTVALUE ||   \
		 (relkind) == RELKIND_MATVIEW)
#endif
