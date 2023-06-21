#include "pgroonga.h"

#include "pgrn-compatible.h"
#include "pgrn-global.h"
#include "pgrn-trace-log.h"
#include "pgrn-value.h"
#include "pgrn-variables.h"
#include "pgrn-wal.h"

#include <utils/guc.h>

#include <groonga.h>

#include <limits.h>

static int PGrnLogType;
enum PGrnLogType {
	PGRN_LOG_TYPE_FILE,
	PGRN_LOG_TYPE_WINDOWS_EVENT_LOG,
	PGRN_LOG_TYPE_POSTGRESQL
};
static struct config_enum_entry PGrnLogTypeEntries[] = {
	{"file",              PGRN_LOG_TYPE_FILE,              false},
	{"windows_event_log", PGRN_LOG_TYPE_WINDOWS_EVENT_LOG, false},
	{"postgresql",        PGRN_LOG_TYPE_POSTGRESQL,        false},
	{NULL,                PGRN_LOG_TYPE_FILE,              false}
};

static char *PGrnLogPath;

static int PGrnLogLevel = GRN_LOG_DEFAULT_LEVEL;
PGRN_DEFINE_LOG_LEVEL_ENTRIES(PGrnLogLevelEntries);

static char *PGrnQueryLogPath;

static int PGrnLockTimeout;

static bool PGrnEnableWAL;
static int PGrnMaxWALSizeKB;

static bool PGrnEnableCrashSafe;

static bool PGrnForceMatchEscalation;

static char *PGrnLibgroongaVersion;

static void
PGrnPostgreSQLLoggerLog(grn_ctx *ctx, grn_log_level level,
						const char *timestamp, const char *title,
						const char *message, const char *location,
						void *user_data)
{
	const char levelMarks[] = " EACewnid-";

	if (location && location[0])
	{
		ereport(LOG,
				(errmsg("pgroonga:log: %s|%c|%s %s %s",
						timestamp, levelMarks[level], title,
						message, location)));
	}
	else
	{
		ereport(LOG,
				(errmsg("pgroonga:log: %s|%c|%s %s",
						timestamp, levelMarks[level], title, message)));
	}
}

static grn_logger PGrnPostgreSQLLogger = {
	GRN_LOG_DEFAULT_LEVEL,
	GRN_LOG_TIME | GRN_LOG_MESSAGE | GRN_LOG_PID,
	NULL,
	PGrnPostgreSQLLoggerLog,
	NULL,
	NULL
};

static void
PGrnLogTypeAssign(int new_value, void *extra)
{
	grn_ctx *ctx = &PGrnContext;

	switch (new_value) {
	case PGRN_LOG_TYPE_WINDOWS_EVENT_LOG:
		grn_windows_event_logger_set(ctx, "PGroonga");
		break;
	case PGRN_LOG_TYPE_POSTGRESQL:
		grn_logger_set(ctx, &PGrnPostgreSQLLogger);
		break;
	default:
		grn_logger_set(ctx, NULL);
		break;
	}
}

static void
PGrnLogPathAssignRaw(const char *new_value)
{
	if (!new_value)
	{
		new_value = PGrnLogPathDefault;
	}

	if (PGrnIsNoneValue(new_value))
	{
		grn_default_logger_set_path(NULL);
	}
	else
	{
		grn_default_logger_set_path(new_value);
	}

	if (PGrnGroongaInitialized) {
		grn_ctx *ctx = &PGrnContext;
		grn_logger_reopen(ctx);
	}
}

static void
PGrnLogPathAssign(const char *new_value, void *extra)
{
	PGrnLogPathAssignRaw(new_value);
}

static void
PGrnLogLevelAssign(int new_value, void *extra)
{
	grn_default_logger_set_max_level(new_value);
}

static void
PGrnQueryLogPathAssignRaw(const char *new_value)
{
	if (!new_value)
	{
		new_value = PGrnQueryLogPathDefault;
	}

	if (PGrnIsNoneValue(new_value))
	{
		grn_default_query_logger_set_path(NULL);
	}
	else
	{
		grn_default_query_logger_set_path(new_value);
	}

	if (PGrnGroongaInitialized)
	{
		grn_ctx *ctx = &PGrnContext;
		grn_query_logger_reopen(ctx);
	}
}

static void
PGrnQueryLogPathAssign(const char *new_value, void *extra)
{
	PGrnQueryLogPathAssignRaw(new_value);
}

static void
PGrnLockTimeoutAssignRaw(int new_value)
{
	grn_set_lock_timeout(new_value);
}

static void
PGrnLockTimeoutAssign(int new_value, void *extra)
{
	PGrnLockTimeoutAssignRaw(new_value);
}

static void
PGrnEnableWALAssign(bool new_value, void *extra)
{
	if (new_value)
	{
		PGrnWALEnable();
	}
	else
	{
		PGrnWALDisable();
	}
}

static void
PGrnMaxWALSizeAssign(int new_value, void *extra)
{
	PGrnWALSetMaxSize(new_value * 1024);
}

static void
PGrnMatchEscalationThresholdAssignRaw(int new_value)
{
	if (!PGrnGroongaInitialized)
		return;

	grn_set_default_match_escalation_threshold(new_value);
	grn_ctx_set_match_escalation_threshold(&PGrnContext, new_value);
}

static void
PGrnMatchEscalationThresholdAssign(int new_value, void *extra)
{
	PGrnMatchEscalationThresholdAssignRaw(new_value);
}

static void
PGrnForceMatchEscalationAssign(bool new_value, void *extra)
{
	if (!PGrnGroongaInitialized)
		return;

	grn_ctx_set_force_match_escalation(&PGrnContext, new_value);
}

static void
PGrnEnableTraceLogAssign(bool newValue, void *extra)
{
	PGrnEnableTraceLog = newValue;
}

void
PGrnInitializeVariables(void)
{
	DefineCustomEnumVariable("pgroonga.log_type",
							 "Log type for PGroonga.",
							 "Available log types: "
							 "[file, windows_event_log, postgresql]. "
							 "The default is file.",
							 &PGrnLogType,
							 PGRN_LOG_TYPE_FILE,
							 PGrnLogTypeEntries,
							 PGC_USERSET,
							 0,
							 NULL,
							 PGrnLogTypeAssign,
							 NULL);

	DefineCustomStringVariable("pgroonga.log_path",
							   "Log path for PGroonga.",
							   "The default is "
							   "\"${PG_DATA}/" PGrnLogPathDefault "\". "
							   "Use \"none\" to disable file output.",
							   &PGrnLogPath,
							   PGrnLogPathDefault,
							   PGC_USERSET,
							   0,
							   NULL,
							   PGrnLogPathAssign,
							   NULL);

	DefineCustomEnumVariable("pgroonga.log_level",
							 "Log level for PGroonga.",
							 "Available log levels: "
							 "[none, emergency, alert, critical, "
							 "error, warning, notice, info, debug, dump]. "
							 "The default is notice.",
							 &PGrnLogLevel,
							 GRN_LOG_DEFAULT_LEVEL,
							 PGrnLogLevelEntries,
							 PGC_USERSET,
							 0,
							 NULL,
							 PGrnLogLevelAssign,
							 NULL);

	DefineCustomStringVariable("pgroonga.query_log_path",
							   "Query log path for PGroonga.",
							   "Path must be a relative path "
							   "from \"${PG_DATA}/\" or absolute path. "
							   "Use \"none\" to disable file output. "
							   "The default is disabled.",
							   &PGrnQueryLogPath,
							   PGrnQueryLogPathDefault,
							   PGC_USERSET,
							   0,
							   NULL,
							   PGrnQueryLogPathAssign,
							   NULL);

	DefineCustomIntVariable("pgroonga.lock_timeout",
							"Try pgroonga.lock_timeout times "
							"at 1 msec intervals to "
							"get write lock in PGroonga.",
							"The default is 900000. "
							"It means that PGroonga tries to get write lock "
							"between about 15 minutes.",
							&PGrnLockTimeout,
							grn_get_lock_timeout(),
							0,
							INT_MAX,
							PGC_USERSET,
							0,
							NULL,
							PGrnLockTimeoutAssign,
							NULL);

	DefineCustomBoolVariable("pgroonga.enable_wal",
							 "Enable WAL.",
							 "You need to enable this "
							 "to use streaming replication.",
							 &PGrnEnableWAL,
							 PGrnWALGetEnabled(),
							 PGC_USERSET,
							 0,
							 NULL,
							 PGrnEnableWALAssign,
							 NULL);

	DefineCustomIntVariable("pgroonga.max_wal_size",
							"Max WAL size in KiB.",
							"If WAL size is larger than this value, old WAL "
							"are overwritten. The default is 0KiB. "
							"It means that no size limit.",
							&PGrnMaxWALSizeKB,
							PGrnWALGetMaxSize() / 1024,
							0,
							MAX_KILOBYTES,
							PGC_USERSET,
							GUC_UNIT_KB,
							NULL,
							PGrnMaxWALSizeAssign,
							NULL);

	DefineCustomBoolVariable("pgroonga.enable_crash_safe",
							 "Enable crash safe feature.",
							 "You also need to add 'pgroonga_crash_safer' to "
							 "#shared_preload_libraries.",
							 &PGrnEnableCrashSafe,
							 false,
							 PGC_USERSET,
							 0,
							 NULL,
							 NULL,
							 NULL);

	DefineCustomIntVariable("pgroonga.match_escalation_threshold",
							"The threshold number of matched records "
							"for determining whether "
							"loose search is used automatically. "
							"-1 disables the auto loose search.",
							"The default is 0. "
							"It means that the number of matched records "
							"is equal or less than 0, loose search is "
							"used automtaically.",
							&PGrnMatchEscalationThreshold,
							grn_get_default_match_escalation_threshold(),
							-1,
							INT_MAX,
							PGC_USERSET,
							0,
							NULL,
							PGrnMatchEscalationThresholdAssign,
							NULL);

	DefineCustomBoolVariable("pgroonga.force_match_escalation",
							 "Whether loose search is always used or "
							 "used conditionally based on "
							 "pgroonga.match_escalation_threshold.",
							 "PGroonga always uses loose search when "
							 "this is on. "
							 "PGroonga determines whether loose search "
							 "is used based on "
							 "pgroonga.match_escalation_threshold when "
							 "this is off. "
							 "The default is off.",
							 &PGrnForceMatchEscalation,
							 false,
							 PGC_USERSET,
							 0,
							 NULL,
							 PGrnForceMatchEscalationAssign,
							 NULL);

	DefineCustomStringVariable("pgroonga.libgroonga_version",
							   "The used libgroonga version.",
							   "It's runtime version "
							   "not compile time version.",
							   &PGrnLibgroongaVersion,
							   grn_get_version(),
							   PGC_INTERNAL,
							   0,
							   NULL,
							   NULL,
							   NULL);

	DefineCustomBoolVariable("pgroonga.enable_trace_log",
							 "Enable trace log.",
							 "Trace logs are logged by NOTICE log level.",
							 &PGrnEnableTraceLog,
							 PGrnEnableTraceLog,
							 PGC_USERSET,
							 0,
							 NULL,
							 PGrnEnableTraceLogAssign,
							 NULL);

	EmitWarningsOnPlaceholders("pgroonga");
}

void
PGrnVariablesApplyInitialValues(void)
{
	grn_ctx_set_force_match_escalation(&PGrnContext, PGrnForceMatchEscalation);
	if (PGrnEnableCrashSafe)
	{
		grn_ctx_set_wal_role(&PGrnContext, GRN_WAL_ROLE_SECONDARY);
	}
}
