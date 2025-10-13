#include "pgroonga.h"

#include "pgrn-groonga.h"
#include "pgrn-language-model-vectorize.h"

#include <catalog/pg_type_d.h>
#include <utils/builtins.h>
#ifdef PGRN_HAVE_VARATT_H
#	include <varatt.h>
#endif

static grn_language_model_loader *loader = NULL;
static grn_obj vector;

PGDLLEXPORT PG_FUNCTION_INFO_V1(pgroonga_language_model_vectorize);

void
PGrnInitializeLanguageModelVectorize(void)
{
	loader = grn_language_model_loader_open(ctx);
	GRN_FLOAT32_INIT(&vector, GRN_OBJ_VECTOR);
}

void
PGrnFinalizeLanguageModelVectorize(void)
{
	GRN_OBJ_FIN(ctx, &vector);
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
	text *modelName = PG_GETARG_TEXT_PP(0);

	grn_language_model *model = NULL;
	grn_language_model_inferencer *inferencer = NULL;

	grn_language_model_loader_set_model(
		ctx, loader, text_to_cstring(modelName), VARSIZE_ANY_EXHDR(modelName));

	model = grn_language_model_loader_load(ctx, loader);
	if (!model)
	{
		PGrnCheckRC(ctx->rc, "%s failed to load model: %s", tag, ctx->errbuf);
	}

	inferencer = grn_language_model_open_inferencer(ctx, model);
	if (!inferencer)
	{
		grn_language_model_close(ctx, model);
		PGrnCheckRC(ctx->rc,
					"%s failed to open model inferencer: %s",
					tag,
					ctx->errbuf);
	}

	GRN_BULK_REWIND(&vector);
	{
		text *target = PG_GETARG_TEXT_PP(1);
		grn_rc rc =
			grn_language_model_inferencer_vectorize(ctx,
													inferencer,
													text_to_cstring(target),
													VARSIZE_ANY_EXHDR(target),
													&vector);
		grn_language_model_inferencer_close(ctx, inferencer);
		grn_language_model_close(ctx, model);

		if (rc != GRN_SUCCESS)
		{
			PGrnCheckRC(
				ctx->rc, "%s failed to vectorize: %s", tag, ctx->errbuf);
		}
	}

	return PGrnConvertToDatum(&vector, FLOAT4ARRAYOID);
}
