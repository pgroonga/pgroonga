#pragma once

#include <stdbool.h>

#include <postgres.h>

#include <fmgr.h>

#include "pgrn-global.h"

extern bool PGrnEnableRLS;
extern bool PGrnIsRLSEnabled;

bool PGrnCheckRLSEnabled(Oid relationID);
bool PGrnCheckRLSEnabledSeqScan(FunctionCallInfo fcinfo);
void PGrnResetRLSEnabled(void);

#define PGRN_RLS_ENABLED_IF(enabled)                                           \
	do                                                                         \
	{                                                                          \
		bool _enabled = PGrnEnableRLS && (enabled);                            \
		if (_enabled)                                                          \
		{                                                                      \
			grn_log_level _logLevel = grn_logger_get_max_level(&PGrnContext);  \
			grn_logger_set_max_level(&PGrnContext, GRN_LOG_CRIT);              \
			PG_TRY()

#define PGRN_RLS_ENABLED_ELSE()                                                \
	PG_CATCH();                                                                \
	{                                                                          \
	}                                                                          \
	PG_END_TRY();                                                              \
	FlushErrorState();                                                         \
	grn_logger_set_max_level(&PGrnContext, _logLevel);                         \
	PGrnResetRLSEnabled();                                                     \
	}                                                                          \
	else                                                                       \
	{

#define PGRN_RLS_ENABLED_END()                                                 \
	}                                                                          \
	}                                                                          \
	while (false)
