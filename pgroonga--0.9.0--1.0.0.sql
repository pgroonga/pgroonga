CREATE FUNCTION pgroonga.canreturn(internal)
	RETURNS internal
	AS 'MODULE_PATHNAME', 'pgroonga_canreturn'
	LANGUAGE C;

UPDATE pg_catalog.pg_am
   SET amcanreturn = 'pgroonga.canreturn'
 WHERE amname = 'pgroonga';
