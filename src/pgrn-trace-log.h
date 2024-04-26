#pragma once

#include "pgrn-global.h"

extern bool PGrnEnableTraceLog;

#define PGRN_TRACE_LOG(status)                                                 \
	do                                                                         \
	{                                                                          \
		if (PGrnEnableTraceLog)                                                \
		{                                                                      \
			GRN_LOG(&PGrnContext,                                              \
					GRN_LOG_NOTICE,                                            \
					"pgroonga: [trace][%s][%s]",                               \
					__func__,                                                  \
					(status));                                                 \
		}                                                                      \
	} while (false)

#define PGRN_TRACE_LOG_ENTER() PGRN_TRACE_LOG("enter")
#define PGRN_TRACE_LOG_EXIT() PGRN_TRACE_LOG("exit")
