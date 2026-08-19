-- Downgrade SQL

DROP FUNCTION IF EXISTS pgroonga_physical_table_names(logical_table_index text, argument_prefix text);
