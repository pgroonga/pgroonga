#pragma once

#ifdef WIN32
#	define PRId64 "I64d"
#	define PRIu64 "I64u"
#	define PGRN_PRIdSIZE "Id"
#	define PGRN_PRIuSIZE "Iu"
#else
#	include <inttypes.h>
#	define PGRN_PRIdSIZE "zd"
#	define PGRN_PRIuSIZE "zu"
#endif

#ifdef GP_VERSION
#	define PGRN_IS_GREENPLUM
#endif

#ifndef PGRN_IS_GREENPLUM
#	define PGRN_SUPPORT_OPTIONS
#	define PGRN_SUPPORT_ENUM_VARIABLE
#	define PGRN_SUPPORT_RECHECK_PER_SCAN
#	define PGRN_SUPPORT_INDEX_ONLY_SCAN
#	define PGRN_SUPPORT_BITMAP_INDEX
#	if PG_VERSION_NUM >= 90400
#		define PGRN_SUPPORT_FILE_NODE_ID_TO_RELATION_ID
#	endif
#endif

#if PG_VERSION_NUM >= 90601
#	define PGRN_FUNCTION_INFO_V1(function_name)		\
	PGDLLEXPORT PG_FUNCTION_INFO_V1(function_name)
#elif PG_VERSION_NUM >= 90600
#	define PGRN_FUNCTION_INFO_V1(function_name)				\
	extern PGDLLEXPORT PG_FUNCTION_INFO_V1(function_name)
#else
#	define PGRN_FUNCTION_INFO_V1(function_name)					\
	extern PGDLLEXPORT Datum function_name(PG_FUNCTION_ARGS);	\
	PG_FUNCTION_INFO_V1(function_name)
#endif

#if PG_VERSION_NUM >= 90600
#	define PGRN_SUPPORT_CREATE_ACCESS_METHOD
#endif

#if PG_VERSION_NUM >= 90600 && defined(PGRN_HAVE_MSGPACK)
#	define PGRN_SUPPORT_WAL
#endif

#if PG_VERSION_NUM >= 90400
#	define PGRN_SUPPORT_JSONB
#endif

#if PG_VERSION_NUM < 110000
#	define PG_GETARG_JSONB_P(n) PG_GETARG_JSONB((n))
#	define DatumGetJsonbP(datum) DatumGetJsonb((datum))
#endif

#if PG_VERSION_NUM >= 110000
typedef const char *PGrnStringOptionValue;
#else
typedef char *PGrnStringOptionValue;
#endif

#ifndef ERRCODE_SYSTEM_ERROR
#	define ERRCODE_SYSTEM_ERROR ERRCODE_IO_ERROR
#endif

#ifndef ALLOCSET_DEFAULT_SIZES
#	define ALLOCSET_DEFAULT_SIZES				\
	ALLOCSET_DEFAULT_MINSIZE,					\
	ALLOCSET_DEFAULT_INITSIZE,					\
	ALLOCSET_DEFAULT_MAXSIZE
#endif

#ifdef PGRN_IS_GREENPLUM
#	define PGrnDefineCustomIntVariable(name,			\
									   shortDesc,		\
									   longDesc,		\
									   valueAddr,		\
									   bootValue,		\
									   minValue,		\
									   maxValue,		\
									   context,			\
									   flags,			\
									   checkHook,		\
									   assignHook,		\
									   showHook)		\
	DefineCustomIntVariable(name,						\
							shortDesc,					\
							longDesc,					\
							valueAddr,					\
							minValue,					\
							maxValue,					\
							context,					\
							assignHook,					\
							showHook)
#	define PGrnDefineCustomStringVariable(name,			\
										  shortDesc,	\
										  longDesc,		\
										  valueAddr,	\
										  bootValue,	\
										  context,		\
										  flags,		\
										  checkHook,	\
										  assignHook,	\
										  showHook)		\
	DefineCustomStringVariable(name,					\
							   shortDesc,				\
							   longDesc,				\
							   valueAddr,				\
							   context,					\
							   assignHook,				\
							   showHook)
#else
#	define PGrnDefineCustomIntVariable(name,			\
									   shortDesc,		\
									   longDesc,		\
									   valueAddr,		\
									   bootValue,		\
									   minValue,		\
									   maxValue,		\
									   context,			\
									   flags,			\
									   checkHook,		\
									   assignHook,		\
									   showHook)		\
	DefineCustomIntVariable(name,						\
							shortDesc,					\
							longDesc,					\
							valueAddr,					\
							bootValue,					\
							minValue,					\
							maxValue,					\
							context,					\
							flags,						\
							checkHook,					\
							assignHook,					\
							showHook)
#	define PGrnDefineCustomStringVariable(name,			\
										  shortDesc,	\
										  longDesc,		\
										  valueAddr,	\
										  bootValue,	\
										  context,		\
										  flags,		\
										  checkHook,	\
										  assignHook,	\
										  showHook)		\
	DefineCustomStringVariable(name,					\
							   shortDesc,				\
							   longDesc,				\
							   valueAddr,				\
							   bootValue,				\
							   context,					\
							   flags,					\
							   checkHook,				\
							   assignHook,				\
							   showHook)
#endif

#ifdef PGRN_IS_GREENPLUM
#	define IndexBuildHeapScan(heapRelation,		\
							  indexRelation,	\
							  indexInfo,		\
							  allow_sync,		\
							  callback,			\
							  callbackState)	\
	IndexBuildScan(heapRelation,				\
				   indexRelation,				\
				   indexInfo,					\
				   callback,					\
				   callbackState)
#endif

#if PG_VERSION_NUM >= 100000
#	define PGRN_AM_INSERT_HAVE_INDEX_INFO
#	define PGRN_AM_COST_ESTIMATE_HAVE_INDEX_PAGES
#	define PGRN_INDEX_AM_ROUTINE_HAVE_AM_CAN_PARALLEL
#	define PGRN_INDEX_AM_ROUTINE_HAVE_AM_ESTIMATE_PARALLEL_SCAN
#	define PGRN_INDEX_AM_ROUTINE_HAVE_AM_INIT_PARALLEL_SCAN
#	define PGRN_INDEX_AM_ROUTINE_HAVE_AM_PARALLEL_RESCAN
#	define PGRN_SUPPORT_LOGICAL_REPLICATION
#endif

#if PG_VERSION_NUM >= 90500
#	define pgrn_array_create_iterator(array, slide_ndim)	\
	array_create_iterator(array, slide_ndim, NULL)
#else
#	define pgrn_array_create_iterator(array, slide_ndim)	\
	array_create_iterator(array, slide_ndim)
#endif

#if PG_VERSION_NUM >= 110000
#	define PGrnIndexBuildHeapScan(heap,				\
								  index,			\
								  indexInfo,		\
								  allowSync,		\
								  callback,			\
								  callbackState)	\
	IndexBuildHeapScan((heap),						\
					   (index),						\
					   (indexInfo),					\
					   (allowSync),					\
					   (callback),					\
					   (callbackState),				\
					   NULL)
#else
#	define PGrnIndexBuildHeapScan(heap,				\
								  index,			\
								  indexInfo,		\
								  allowSync,		\
								  callback,			\
								  callbackState)	\
	IndexBuildHeapScan((heap),						\
					   (index),						\
					   (indexInfo),					\
					   (allowSync),					\
					   (callback),					\
					   (callbackState))
#endif
