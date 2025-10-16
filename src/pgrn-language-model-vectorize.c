#include "pgroonga.h"

#include "pgrn-groonga.h"
#include "pgrn-language-model-vectorize.h"

#include <catalog/pg_type_d.h>
#include <utils/builtins.h>
#ifdef PGRN_HAVE_VARATT_H
#	include <varatt.h>
#endif

static grn_language_model_loader *loader = NULL;
static grn_language_model *model = NULL;
static grn_language_model_inferencer *inferencer = NULL;
static char *currentModelName = NULL;

static grn_obj vector;

PGDLLEXPORT PG_FUNCTION_INFO_V1(pgroonga_language_model_vectorize);

void
PGrnInitializeLanguageModelVectorize(void)
{
	loader = grn_language_model_loader_open(ctx);
	GRN_FLOAT32_INIT(&vector, GRN_OBJ_VECTOR);
}

static void
PGrnLanguageModelClose(void)
{
	if (inferencer)
	{
		grn_language_model_inferencer_close(ctx, inferencer);
		inferencer = NULL;
	}
	if (model)
	{
		grn_language_model_close(ctx, model);
		model = NULL;
	}
}

static grn_rc
PGrnLanguageModelLoad(const char *modelName)
{
	grn_language_model_loader_set_model(
		ctx, loader, modelName, strlen(modelName));

	model = grn_language_model_loader_load(ctx, loader);
	if (!model)
		return ctx->rc;

	inferencer = grn_language_model_open_inferencer(ctx, model);
	return ctx->rc;
}

void
PGrnFinalizeLanguageModelVectorize(void)
{
	GRN_OBJ_FIN(ctx, &vector);
	PGrnLanguageModelClose();
	if (loader)
	{
		grn_language_model_loader_close(ctx, loader);
		loader = NULL;
	}
}

Datum
pgroonga_language_model_vectorize(PG_FUNCTION_ARGS)
{
	const char *tag = "[language-model-vectorize]";
	const char *modelName = PG_GETARG_CSTRING(0);

	if (currentModelName && strcmp(currentModelName, modelName) != 0)
	{
		currentModelName = NULL;
	}

	if (!currentModelName)
	{
		PGrnLanguageModelClose();
		if (PGrnLanguageModelLoad(modelName) != GRN_SUCCESS)
			PGrnCheck("%s[model][init]", tag);

		currentModelName = (char *) palloc(strlen(modelName) + 1);
		currentModelName = pstrdup(modelName);
	}

	GRN_BULK_REWIND(&vector);
	{
		text *target = PG_GETARG_TEXT_PP(1);
		grn_rc rc =
			grn_language_model_inferencer_vectorize(ctx,
													inferencer,
													VARDATA_ANY(target),
													VARSIZE_ANY_EXHDR(target),
													&vector);

		if (rc != GRN_SUCCESS)
			PGrnCheck("%s[vectorize]", tag);
	}

	return PGrnConvertToDatum(&vector, FLOAT4ARRAYOID);
}
