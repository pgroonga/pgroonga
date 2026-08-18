-- Upgrade SQL

CREATE OR REPLACE FUNCTION pgroonga_physical_table_names(logical_table_name text, argument_prefix text)
	RETURNS text[]
	AS 'MODULE_PATHNAME', 'pgroonga_physical_table_names'
	LANGUAGE C
	STRICT
	PARALLEL SAFE;
