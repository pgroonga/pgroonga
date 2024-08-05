-- Upgrade SQL

-- Two pgroonga_condition() were defined by a bug in `data/pgroonga--3.2.0--3.2.1.sql`.
-- Remove unnecessary definitions.
DROP FUNCTION IF EXISTS pgroonga_condition(query text,
					   weights int[],
					   scorers text[],
					   schema_name text,
					   index_name text,
					   column_name text);
