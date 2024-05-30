-- Downgrade SQL

ALTER TYPE pgroonga_condition DROP ATTRIBUTE fuzzy_max_distance_ratio;
CREATE OR REPLACE FUNCTION pgroonga_condition(query text = null,
				   weights int[] = null,
				   scorers text[] = null,
				   schema_name text = null,
				   index_name text = null,
				   column_name text = null)
	RETURNS pgroonga_condition
	LANGUAGE SQL
	AS $$
		SELECT (
			query,
			weights,
			scorers,
			schema_name,
			index_name,
			column_name
		)::pgroonga_condition
	$$
	IMMUTABLE
	LEAKPROOF
	PARALLEL SAFE;

DROP FUNCTION pgroonga_list_lagged_indexes;
DROP FUNCTION pgroonga_list_broken_indexes;
