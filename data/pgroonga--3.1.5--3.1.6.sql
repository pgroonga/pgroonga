-- Upgrade SQL

CREATE TYPE pgroonga_condition AS (
	query text,
	weigths int[],
	scorers text[],
	schema_name text,
	index_name text,
	column_name text
);

CREATE FUNCTION pgroonga_condition(query text = null,
				   weights int[] = null,
				   scorers text[] = null,
				   schema_name text = null,
				   index_name text = null,
				   column_name text = null)
	RETURNS pgroonga_condition
	RETURN (
		query,
		weights,
		scorers,
		schema_name,
		index_name,
		column_name
	)::pgroonga_condition;
