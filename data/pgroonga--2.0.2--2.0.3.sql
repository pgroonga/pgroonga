-- Set RESTRICT and JOIN
UPDATE pg_catalog.pg_operator
   SET oprrest = 'contsel',
       oprjoin = 'contjoinsel'
 WHERE oprcode::text LIKE 'pgroonga_%' OR
       oprcode::text LIKE 'public.pgroonga_%' OR
       oprcode::text LIKE 'pgroonga.%';

CREATE FUNCTION pgroonga_normalize(value text)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_normalize'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga_normalize(value text, normalizerName text)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_normalize'
	LANGUAGE C
	IMMUTABLE
	STRICT;
