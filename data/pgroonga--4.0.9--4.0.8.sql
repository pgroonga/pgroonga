-- Downgrade SQL

DROP FUNCTION IF EXISTS pgroonga_physical_table_names(logical_index_name text, argument_name text);
