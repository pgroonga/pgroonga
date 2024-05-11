#pragma once

#include "pgrn-row-level-security.h"

#include <groonga.h>

#include <c.h>

static inline int
PGrnGrnRCToPGErrorCode(grn_rc rc)
{
	int errorCode = ERRCODE_SYSTEM_ERROR;

	/* TODO: Fill me. */
	switch (rc)
	{
	case GRN_NO_SUCH_FILE_OR_DIRECTORY:
		errorCode = ERRCODE_IO_ERROR;
		break;
	case GRN_INPUT_OUTPUT_ERROR:
		errorCode = ERRCODE_IO_ERROR;
		break;
	case GRN_INVALID_ARGUMENT:
		errorCode = ERRCODE_INVALID_PARAMETER_VALUE;
		break;
	case GRN_FUNCTION_NOT_IMPLEMENTED:
		errorCode = ERRCODE_FEATURE_NOT_SUPPORTED;
		break;
	case GRN_NO_MEMORY_AVAILABLE:
		errorCode = ERRCODE_OUT_OF_MEMORY;
		break;
	default:
		break;
	}

	return errorCode;
}

static inline bool PGrnCheck(const char *format, ...) GRN_ATTRIBUTE_PRINTF(1);
static inline bool
PGrnCheck(const char *format, ...)
{
#define MESSAGE_SIZE 4096
	va_list args;
	char message[MESSAGE_SIZE];

	if (ctx->rc == GRN_SUCCESS)
		return true;

#ifdef PGRN_MODULE_PGROONGA
	if (PGrnIsRLSEnabled)
		PG_RE_THROW();
#endif

	va_start(args, format);
	grn_vsnprintf(message, MESSAGE_SIZE, format, args);
	va_end(args);
	ereport(ERROR,
			(errcode(PGrnGrnRCToPGErrorCode(ctx->rc)),
			 errmsg("%s: %s: %s", PGRN_TAG, message, ctx->errbuf)));
	return false;
#undef MESSAGE_SIZE
}

static inline bool PGrnCheckRC(grn_rc rc, const char *format, ...)
	GRN_ATTRIBUTE_PRINTF(2);
static inline bool
PGrnCheckRC(grn_rc rc, const char *format, ...)
{
#define MESSAGE_SIZE 4096
	va_list args;
	char message[MESSAGE_SIZE];

	if (rc == GRN_SUCCESS)
		return true;

#ifdef PGRN_MODULE_PGROONGA
	if (PGrnIsRLSEnabled)
		PG_RE_THROW();
#endif

	va_start(args, format);
	grn_vsnprintf(message, MESSAGE_SIZE, format, args);
	va_end(args);
	ereport(ERROR,
			(errcode(PGrnGrnRCToPGErrorCode(rc)),
			 errmsg("%s: %s", PGRN_TAG, message)));
	return false;
#undef MESSAGE_SIZE
}

static inline bool
PGrnCheckRCLevel(grn_rc rc, int errorLevel, const char *format, ...)
	GRN_ATTRIBUTE_PRINTF(3);
static inline bool
PGrnCheckRCLevel(grn_rc rc, int errorLevel, const char *format, ...)
{
#define MESSAGE_SIZE 4096
	va_list args;
	char message[MESSAGE_SIZE];

	if (rc == GRN_SUCCESS)
		return true;

#ifdef PGRN_MODULE_PGROONGA
	if (PGrnIsRLSEnabled)
	{
		if (errorLevel == ERROR)
		{
			PG_RE_THROW();
		}
		else
		{
			return false;
		}
	}
#endif

	va_start(args, format);
	grn_vsnprintf(message, MESSAGE_SIZE, format, args);
	va_end(args);
	ereport(errorLevel,
			(errcode(PGrnGrnRCToPGErrorCode(rc)),
			 errmsg("%s: %s", PGRN_TAG, message)));
	return false;
#undef MESSAGE_SIZE
}

