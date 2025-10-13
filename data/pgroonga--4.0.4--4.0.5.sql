-- Upgrade SQL

CREATE FUNCTION pgroonga_language_model_vectorize(model_name text, target text)
	RETURNS float4[]
	AS 'MODULE_PATHNAME', 'pgroonga_language_model_vectorize'
	LANGUAGE C
	STRICT
	PARALLEL SAFE;
