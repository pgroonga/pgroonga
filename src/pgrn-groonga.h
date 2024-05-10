#pragma once

#include <c.h>
#include <mb/pg_wchar.h>
#include <postgres.h>
#include <utils/rel.h>

#include <groonga.h>

extern bool PGrnIsLZ4Available;
extern bool PGrnIsZlibAvailable;
extern bool PGrnIsZstdAvailable;
extern bool PGrnIsTemporaryIndexSearchAvailable;

void PGrnInitializeGroongaInformation(void);

const char *PGrnInspect(grn_obj *object);
const char *PGrnInspectName(grn_obj *object);

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

bool PGrnCheck(const char *format, ...) GRN_ATTRIBUTE_PRINTF(1);
bool PGrnCheckRC(grn_rc rc, const char *format, ...) GRN_ATTRIBUTE_PRINTF(2);
bool PGrnCheckRCLevel(grn_rc rc, int errorLevel, const char *format, ...)
	GRN_ATTRIBUTE_PRINTF(3);

static inline grn_encoding
PGrnPGEncodingToGrnEncoding(int pgEncoding, const char *tag)
{
	switch (pgEncoding)
	{
	case PG_SQL_ASCII:
	case PG_UTF8:
		return GRN_ENC_UTF8;
	case PG_EUC_JP:
	case PG_EUC_JIS_2004:
		return GRN_ENC_EUC_JP;
	case PG_LATIN1:
	case PG_WIN1252:
		return GRN_ENC_LATIN1;
	case PG_KOI8R:
		return GRN_ENC_KOI8R;
	case PG_SJIS:
	case PG_SHIFT_JIS_2004:
		return GRN_ENC_SJIS;
	default:
		ereport(WARNING,
				errmsg("%s: use default encoding instead of '%s'",
					   tag,
					   pg_encoding_to_char(pgEncoding)));
		return GRN_ENC_DEFAULT;
	}
}

grn_obj *PGrnLookup(const char *name, int errorLevel);
grn_obj *PGrnLookupWithSize(const char *name, size_t nameSize, int errorLevel);
grn_obj *PGrnLookupColumn(grn_obj *table, const char *name, int errorLevel);
grn_obj *PGrnLookupColumnWithSize(grn_obj *table,
								  const char *name,
								  size_t nameSize,
								  int errorLevel);
grn_obj *PGrnLookupSourcesTable(Relation index, int errorLevel);
grn_obj *PGrnLookupSourcesCtidColumn(Relation index, int errorLevel);
grn_obj *
PGrnLookupLexicon(Relation index, unsigned int nthAttribute, int errorLevel);
grn_obj *PGrnLookupIndexColumn(Relation index,
							   unsigned int nthAttribute,
							   int errorLevel);

void PGrnFormatSourcesTableName(const char *indexName,
								char output[GRN_TABLE_MAX_KEY_SIZE]);

grn_obj *PGrnCreateTable(Relation index,
						 const char *name,
						 grn_table_flags flags,
						 grn_obj *type,
						 grn_obj *tokenizer,
						 grn_obj *normalizers,
						 grn_obj *tokenFilters);
grn_obj *PGrnCreateTableWithSize(Relation index,
								 const char *name,
								 size_t nameSize,
								 grn_table_flags flags,
								 grn_obj *type,
								 grn_obj *tokenizer,
								 grn_obj *normalizers,
								 grn_obj *tokenFilters);
grn_obj *PGrnCreateSimilarTemporaryLexicon(Relation index,
										   const char *attributeName,
										   size_t attributeNameSize,
										   const char *tag);
grn_obj *PGrnCreateColumn(Relation index,
						  grn_obj *table,
						  const char *name,
						  grn_column_flags flags,
						  grn_obj *type);
grn_obj *PGrnCreateColumnWithSize(Relation index,
								  grn_obj *table,
								  const char *name,
								  size_t nameSize,
								  grn_column_flags flags,
								  grn_obj *type);

void PGrnIndexColumnClearSources(Relation index, grn_obj *indexColumn);
void
PGrnIndexColumnSetSource(Relation index, grn_obj *indexColumn, grn_obj *source);
void PGrnIndexColumnSetSourceIDs(Relation index,
								 grn_obj *indexColumn,
								 grn_obj *sourceIDs);

void PGrnRenameTable(Relation index, grn_obj *table, const char *newName);

void PGrnRemoveObject(const char *name);
void PGrnRemoveObjectWithSize(const char *name, size_t nameSize);
void PGrnRemoveObjectForce(const char *name);
void PGrnRemoveObjectForceWithSize(const char *name, size_t nameSize);

void PGrnRemoveColumns(grn_obj *table);

void PGrnFlushObject(grn_obj *object, bool recursive);

grn_id PGrnPGTypeToGrnType(Oid pgTypeID, unsigned char *flags);
Oid PGrnGrnTypeToPGType(grn_id typeID);
