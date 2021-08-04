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
static grn_obj tokenMetadataName;
static grn_obj tokenMetadataValue;
static grn_obj tokenJSON;

PGDLLEXPORT PG_FUNCTION_INFO_V1(pgroonga_tokenize);

typedef struct {
	grn_id id;
	grn_obj value;
	int32_t position;
	grn_bool forcePrefixSearch;
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
		GRN_OBJ_FIN(ctx, &(token->value));
		GRN_OBJ_FIN(ctx, &(token->metadata));
	}
	GRN_BULK_REWIND(&tokens);
}

static void
PGrnTokensAppend(grn_id id, grn_token_cursor *tokenCursor)
{
	PGrnToken *token;
	grn_token *grnToken;

	grn_bulk_space(ctx, &tokens, sizeof(PGrnToken));
	token = ((PGrnToken *) (GRN_BULK_CURR(&tokens))) - 1;
	GRN_TEXT_INIT(&(token->value), 0);
	GRN_TEXT_INIT(&(token->metadata), GRN_OBJ_VECTOR);

	token->id = id;

	grnToken = grn_token_cursor_get_token(ctx, tokenCursor);
	{
		grn_obj *data = grn_token_get_data(ctx, grnToken);
		GRN_TEXT_SET(ctx,
					 &(token->value),
					 GRN_TEXT_VALUE(data),
					 GRN_TEXT_LEN(data));
	}
	token->position = grn_token_get_position(ctx, grnToken);
	token->forcePrefixSearch = grn_token_get_position(ctx, grnToken);
    token->sourceOffset = grn_token_get_source_offset(ctx, grnToken);
    token->sourceLength = grn_token_get_source_length(ctx, grnToken);
    token->sourceFirstCharacterLength =
      grn_token_get_source_first_character_length(ctx, grnToken);
    {
      grn_obj *metadata;
      size_t nMetadata;
      size_t i;

      metadata = grn_token_get_metadata(ctx, grnToken);
      nMetadata = grn_token_metadata_get_size(ctx, metadata);
      for (i = 0; i < nMetadata; i++) {
        GRN_BULK_REWIND(&tokenMetadataName);
        GRN_BULK_REWIND(&tokenMetadataValue);
        grn_token_metadata_at(ctx,
							  metadata,
							  i,
							  &tokenMetadataName,
							  &tokenMetadataValue);
        if (GRN_TEXT_LEN(&tokenMetadataName) == 0) {
          continue;
        }
        grn_vector_add_element(ctx,
                               &(token->metadata),
                               GRN_BULK_HEAD(&tokenMetadataName),
                               GRN_BULK_VSIZE(&tokenMetadataName),
                               0,
                               tokenMetadataName.header.domain);
        grn_vector_add_element(ctx,
                               &(token->metadata),
                               GRN_BULK_HEAD(&tokenMetadataValue),
                               GRN_BULK_VSIZE(&tokenMetadataValue),
                               0,
                               tokenMetadataValue.header.domain);
      }
    }
}

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
	GRN_TEXT_INIT(&tokenMetadataName, 0);
	GRN_VOID_INIT(&tokenMetadataValue);
	GRN_TEXT_INIT(&tokenJSON, 0);
}

void
PGrnFinalizeTokenize(void)
{
	GRN_OBJ_FIN(ctx, &tokenJSON);
	GRN_OBJ_FIN(ctx, &tokenMetadataValue);
	GRN_OBJ_FIN(ctx, &tokenMetadataName);
	PGrnTokensFin();
	GRN_OBJ_FIN(ctx, &tokenFiltersValue);
	GRN_OBJ_FIN(ctx, &normalizerValue);
	GRN_OBJ_FIN(ctx, &tokenizerValue);
	grn_obj_close(ctx, lexicon);
}

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
PGrnTokenizeCreateArray(void)
{
	size_t i;
	size_t nTokens;
	Datum *tokenData;
	int	dims[1];
	int	lbs[1];

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
		int nElements = 3;
		bool haveSourceLocation = false;
		bool haveMetadata = false;
		text *json;

		GRN_BULK_REWIND(&tokenJSON);
		if (token->sourceOffset > 0 || token->sourceLength > 0)
		{
			haveSourceLocation = true;
			nElements += 3;
		}
		if (grn_vector_size(ctx, &(token->metadata)) > 0)
		{
			haveMetadata = true;
			nElements++;
		}
		grn_output_map_open(ctx, &tokenJSON, type, "token", nElements);
		grn_output_cstr(ctx, &tokenJSON, type, "value");
		grn_output_str(ctx, &tokenJSON, type,
					   GRN_TEXT_VALUE(&(token->value)),
					   GRN_TEXT_LEN(&(token->value)));
		grn_output_cstr(ctx, &tokenJSON, type, "position");
		grn_output_uint32(ctx, &tokenJSON, type, token->position);
		grn_output_cstr(ctx, &tokenJSON, type, "force_prefix_search");
		grn_output_bool(ctx, &tokenJSON, type, token->forcePrefixSearch);
		if (haveSourceLocation)
		{
			grn_output_cstr(ctx, &tokenJSON, type, "source_offset");
			grn_output_uint64(ctx, &tokenJSON, type, token->sourceOffset);
			grn_output_cstr(ctx, &tokenJSON, type, "source_length");
			grn_output_uint32(ctx, &tokenJSON, type, token->sourceLength);
			grn_output_cstr(ctx, &tokenJSON, type,
							"source_first_character_length");
			grn_output_uint32(ctx, &tokenJSON, type,
							  token->sourceFirstCharacterLength);
		}
		if (haveMetadata)
		{
			size_t j;
			size_t nMetadata;

			nMetadata = grn_vector_size(ctx, &(token->metadata)) / 2;
			grn_output_cstr(ctx, &tokenJSON, type, "metadata");
			grn_output_map_open(ctx, &tokenJSON, type, "metadata", nMetadata);
			for (j = 0; j < nMetadata; j++)
			{
				const char *rawName;
				unsigned int rawNameLength;
				const char *rawValue;
				unsigned int rawValueLength;
				grn_id valueDomain;

				rawNameLength = grn_vector_get_element(ctx,
													   &(token->metadata),
													   j * 2,
													   &rawName,
													   NULL,
													   NULL);
				grn_output_str(ctx, &tokenJSON, type, rawName, rawNameLength);

				rawValueLength = grn_vector_get_element(ctx,
														&(token->metadata),
														j * 2 + 1,
														&rawValue,
														NULL,
														&valueDomain);
				grn_obj_reinit(ctx, &tokenMetadataValue, valueDomain, 0);
				grn_bulk_write(ctx, &tokenMetadataValue,
							   rawValue, rawValueLength);
				grn_output_obj(ctx, &tokenJSON, type, &tokenMetadataValue, NULL);
			}
			grn_output_map_close(ctx, &tokenJSON, type);
		}
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

static ArrayType *
PGrnTokenize(text *target)
{
	grn_token_cursor *tokenCursor;

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

	return PGrnTokenizeCreateArray();
}

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
}
