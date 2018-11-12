#include "pgroonga.h"
#include "pgrn-compatible.h"
#include "pgrn-global.h"
#include "pgrn-groonga.h"
#include "pgrn-tokenize.h"

#include <catalog/pg_type.h>
#include <utils/array.h>
#include <utils/builtins.h>
#include <utils/json.h>

static grn_ctx *ctx = &PGrnContext;
static grn_obj *lexicon;
static grn_obj tokenizerValue;
static grn_obj normalizerValue;
static grn_obj tokenFiltersValue;
static grn_obj tokens;
static grn_obj tokenJSON;

PGRN_FUNCTION_INFO_V1(pgroonga_tokenize);

typedef struct {
	grn_id id;
	grn_obj data;
	int32_t position;
	grn_bool forcePrefix;
	uint64_t sourceOffset;
	uint32_t sourceLength;
	uint32_t sourceFirstCharacterLength;
	grn_obj metadata;
} PGrnToken;

static void
PGrnTokensInit(void)
{
	GRN_VALUE_FIX_SIZE_INIT(&tokens, GRN_OBJ_VECTOR, GRN_ID_NIL);
}

static size_t
PGrnTokensSize(void)
{
	return GRN_BULK_VSIZE(&tokens) / sizeof(PGrnToken);
}

static PGrnToken *
PGrnTokensAt(size_t i)
{
	PGrnToken *rawTokens;
	rawTokens = (PGrnToken *) GRN_BULK_HEAD(&tokens);
	return rawTokens + i;
}

static void
PGrnTokensReinit(void)
{
	size_t i;
	size_t nTokens;

	nTokens = PGrnTokensSize();
	for (i = 0; i < nTokens; i++)
	{
		PGrnToken *token;
		token = PGrnTokensAt(i);
		GRN_OBJ_FIN(ctx, &(token->data));
		GRN_OBJ_FIN(ctx, &(token->metadata));
	}
	GRN_BULK_REWIND(&tokens);
}

#if GRN_VERSION_OR_LATER(8, 0, 9)
static void
PGrnTokensAppend(grn_id id, grn_token_cursor *tokenCursor)
{
	PGrnToken *token;
	grn_token *grnToken;
	grn_obj *data;

	grn_bulk_space(ctx, &tokens, sizeof(PGrnToken));
	token = ((PGrnToken *) (GRN_BULK_CURR(&tokens))) - 1;
	GRN_TEXT_INIT(&(token->data), 0);
	GRN_TEXT_INIT(&(token->metadata), GRN_OBJ_VECTOR);

	grnToken = grn_token_cursor_get_token(ctx, tokenCursor);
	data = grn_token_get_data(ctx, grnToken);
	GRN_TEXT_SET(ctx,
				 &(token->data),
				 GRN_TEXT_VALUE(data),
				 GRN_TEXT_LEN(data));
}
#endif

static void
PGrnTokensFin(void)
{
	PGrnTokensReinit();
	GRN_OBJ_FIN(ctx, &tokens);
}

void
PGrnInitializeTokenize(void)
{
	lexicon = grn_table_create(ctx, NULL, 0, NULL,
							   GRN_OBJ_TABLE_PAT_KEY,
							   grn_ctx_at(ctx, GRN_DB_SHORT_TEXT),
							   NULL);
	GRN_TEXT_INIT(&tokenizerValue, 0);
	GRN_TEXT_INIT(&normalizerValue, 0);
	GRN_TEXT_INIT(&tokenFiltersValue, 0);
	PGrnTokensInit();
	GRN_TEXT_INIT(&tokenJSON, 0);
}

void
PGrnFinalizeTokenize(void)
{
	GRN_OBJ_FIN(ctx, &tokenJSON);
	PGrnTokensFin();
	GRN_OBJ_FIN(ctx, &tokenFiltersValue);
	GRN_OBJ_FIN(ctx, &normalizerValue);
	GRN_OBJ_FIN(ctx, &tokenizerValue);
	grn_obj_close(ctx, lexicon);
}

#if GRN_VERSION_OR_LATER(8, 0, 9)
static void
PGrnTokenizeSetModule(const char *moduleName,
					  grn_info_type type,
					  text *newValue)
{
	grn_obj *value;

	switch (type)
	{
	case GRN_INFO_DEFAULT_TOKENIZER:
		value = &tokenizerValue;
		break;
	case GRN_INFO_NORMALIZER:
		value = &normalizerValue;
		break;
	case GRN_INFO_TOKEN_FILTERS:
		value = &tokenFiltersValue;
		break;
	default:
		PGrnCheck("tokenize: invalid %s type: <%d>", moduleName, type);
		return;
	}

	if (newValue)
	{
		if (VARSIZE_ANY_EXHDR(newValue) == GRN_TEXT_LEN(value) &&
			memcmp(VARDATA_ANY(newValue),
				   GRN_TEXT_VALUE(value),
				   GRN_TEXT_LEN(value)) == 0)
		{
			return;
		}

		GRN_TEXT_SET(ctx,
					 value,
					 VARDATA_ANY(newValue),
					 VARSIZE_ANY_EXHDR(newValue));
		grn_obj_set_info(ctx, lexicon, type, value);
		PGrnCheck("tokenize: failed to set %s", moduleName);
	}
	else
	{
		if (GRN_TEXT_LEN(value) == 0)
			return;

		GRN_BULK_REWIND(value);
		grn_obj_set_info(ctx, lexicon, type, value);
		PGrnCheck("tokenize: failed to set %s", moduleName);
	}
}

static ArrayType *
PGrnTokenize(text *target)
{
	grn_token_cursor *tokenCursor;
	size_t i;
	size_t nTokens;
	Datum *tokenData;
	int	dims[1];
	int	lbs[1];

	tokenCursor = grn_token_cursor_open(ctx,
										lexicon,
										VARDATA_ANY(target),
										VARSIZE_ANY_EXHDR(target),
										GRN_TOKEN_ADD,
										0);
	PGrnCheck("tokenize: failed to create token cursor");

	PGrnTokensReinit();
	while (grn_token_cursor_get_status(ctx, tokenCursor) ==
		   GRN_TOKEN_CURSOR_DOING)
	{
		grn_id id = grn_token_cursor_next(ctx, tokenCursor);

		if (id == GRN_ID_NIL)
			continue;

		PGrnTokensAppend(id, tokenCursor);
	}
	grn_token_cursor_close(ctx, tokenCursor);

	nTokens = PGrnTokensSize();
	if (nTokens == 0)
	{
		return construct_empty_array(JSONOID);
	}

	tokenData = palloc(sizeof(Datum) * nTokens);
	for (i = 0; i < nTokens; i++)
	{
		grn_content_type type = GRN_CONTENT_JSON;
		PGrnToken *token = PGrnTokensAt(i);
		text *json;

		GRN_BULK_REWIND(&tokenJSON);
		grn_output_map_open(ctx, &tokenJSON, type, "token", 1);
		grn_output_cstr(ctx, &tokenJSON, type, "data");
		grn_output_str(ctx, &tokenJSON, type,
					   GRN_TEXT_VALUE(&(token->data)),
					   GRN_TEXT_LEN(&(token->data)));
		grn_output_map_close(ctx, &tokenJSON, type);
		json = cstring_to_text_with_len(GRN_TEXT_VALUE(&tokenJSON),
										GRN_TEXT_LEN(&tokenJSON));
		tokenData[i] = PointerGetDatum(json);
	}
	dims[0] = nTokens;
	lbs[0] = 1;
	return construct_md_array(tokenData,
							  NULL,
							  1,
							  dims,
							  lbs,
							  JSONOID,
							  -1,
							  false,
							  'i');
}
#endif

/**
 * pgroonga_tokenize(target text, options text[]) : json[]
 *
 * options:
 *   "tokenizer", tokenizer text,
 *   "normalizer", normalizer text,
 *   "token_filters", token_filters text,
 *   ...
 */
Datum
pgroonga_tokenize(PG_FUNCTION_ARGS)
{
/* TODO: Remove me and require Groonga 8.0.9 when Groonga 8.0.9 is released. */
#if GRN_VERSION_OR_LATER(8, 0, 9)
	text *target;
	ArrayType *options;
	text *tokenizerName = NULL;
	text *normalizerName = NULL;
	text *tokenFiltersName = NULL;
	ArrayType *pgTokens;

	target = PG_GETARG_TEXT_PP(0);
	options = PG_GETARG_ARRAYTYPE_P(1);

	if (ARR_NDIM(options) > 0)
	{
		ArrayIterator iterator;
		Datum nameDatum;
		bool isNULL;

		iterator = pgrn_array_create_iterator(options, 0);
		while (array_iterate(iterator, &nameDatum, &isNULL))
		{
			text *name = DatumGetTextPP(nameDatum);
			Datum valueDatum;
			text *value;

			if (!array_iterate(iterator, &valueDatum, &isNULL))
			{
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						 errmsg("pgroonga: tokenize: "
								"parameter value is missing: <%.*s>",
								(int) VARSIZE_ANY_EXHDR(name),
								VARDATA_ANY(name))));
			}

			value = DatumGetTextPP(valueDatum);

#define NAME_EQUAL(n)									\
			(VARSIZE_ANY_EXHDR(name) == strlen(n) &&	\
			 strcmp(VARDATA_ANY(name), n) == 0)

			if (NAME_EQUAL("tokenizer"))
			{
				tokenizerName = value;
			}
			else if (NAME_EQUAL("normalizer"))
			{
				normalizerName = value;
			}
			else if (NAME_EQUAL("token_filters"))
			{
				tokenFiltersName = value;
			}
			else
			{
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						 errmsg("pgroonga: tokenize: "
								"unknown parameter name: <%.*s>",
								(int) VARSIZE_ANY_EXHDR(name),
								VARDATA_ANY(name))));
			}
#undef NAME_EQUAL
		}

		array_free_iterator(iterator);
    }

	PGrnTokenizeSetModule("tokenizer",
						  GRN_INFO_DEFAULT_TOKENIZER,
						  tokenizerName);
	PGrnTokenizeSetModule("normalizer",
						  GRN_INFO_NORMALIZER,
						  normalizerName);
	PGrnTokenizeSetModule("token filters",
						  GRN_INFO_TOKEN_FILTERS,
						  tokenFiltersName);

	pgTokens = PGrnTokenize(target);

	PG_RETURN_POINTER(pgTokens);
#else
	PG_RETURN_POINTER(construct_empty_array(JSONOID));
#endif
}
