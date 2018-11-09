#include "pgroonga.h"
#include "pgrn-compatible.h"
#include "pgrn-global.h"
#include "pgrn-groonga.h"
#include "pgrn-tokenize.h"

#include <catalog/pg_type.h>
#include <utils/array.h>
#include <utils/builtins.h>

static grn_ctx *ctx = &PGrnContext;
static grn_obj *lexicon;
static grn_obj tokenizerValue;
static grn_obj normalizerValue;
static grn_obj tokenFiltersValue;
static grn_obj tokens;

PGRN_FUNCTION_INFO_V1(pgroonga_tokenize);

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
	GRN_TEXT_INIT(&tokens, GRN_OBJ_VECTOR);
}

void
PGrnFinalizeTokenize(void)
{
	GRN_OBJ_FIN(ctx, &tokens);
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

	GRN_BULK_REWIND(&tokens);
	while (grn_token_cursor_get_status(ctx, tokenCursor) ==
		   GRN_TOKEN_CURSOR_DOING)
	{
		grn_id id = grn_token_cursor_next(ctx, tokenCursor);
		grn_token *token;
		grn_obj *data;

		if (id == GRN_ID_NIL)
			continue;

		token = grn_token_cursor_get_token(ctx, tokenCursor);
		data = grn_token_get_data(ctx, token);
		grn_vector_add_element(ctx,
							   &tokens,
							   GRN_TEXT_VALUE(data),
							   GRN_TEXT_LEN(data),
							   0,
							   GRN_DB_TEXT);
	}

	grn_token_cursor_close(ctx, tokenCursor);

	nTokens = grn_vector_size(ctx, &tokens);
	if (nTokens == 0)
	{
		return construct_empty_array(TEXTOID);
	}

	tokenData = palloc(sizeof(Datum) * nTokens);
	for (i = 0; i < nTokens; i++)
	{
		const char *data;
		unsigned int length;

		length = grn_vector_get_element(ctx, &tokens, i, &data, NULL, NULL);
		tokenData[i] = PointerGetDatum(cstring_to_text_with_len(data, length));
	}
	dims[0] = nTokens;
	lbs[0] = 1;
	return construct_md_array(tokenData,
							  NULL,
							  1,
							  dims,
							  lbs,
							  TEXTOID,
							  -1,
							  false,
							  'i');
}
#endif

/**
 * pgroonga_tokenize(target text, options text[]) : text[]
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
	PG_RETURN_POINTER(construct_empty_array(TEXTOID));
#endif
}
