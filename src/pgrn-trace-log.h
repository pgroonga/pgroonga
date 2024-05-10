#pragma once

#include <stdbool.h>

extern bool PGrnEnableTraceLog;

#define PGRN_TRACE_LOG(status)                                                 \
	do                                                                         \
	{                                                                          \
		if (PGrnEnableTraceLog)                                                \
		{                                                                      \
			GRN_LOG(ctx,                                                       \
					GRN_LOG_NOTICE,                                            \
					"%s: [trace][%s][%s]",                                     \
					PGRN_TAG,                                                  \
					__func__,                                                  \
					(status));                                                 \
		}                                                                      \
	} while (false)

#define PGRN_TRACE_LOG_ENTER() PGRN_TRACE_LOG("enter")
#define PGRN_TRACE_LOG_EXIT() PGRN_TRACE_LOG("exit")
