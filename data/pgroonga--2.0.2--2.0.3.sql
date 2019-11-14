-- Set RESTRICT and JOIN
UPDATE pg_catalog.pg_operator
   SET oprrest = 'contsel',
       oprjoin = 'contjoinsel'
 WHERE oprcode::text LIKE 'pgroonga_%' OR
       oprcode::text LIKE 'public.pgroonga_%' OR
       oprcode::text LIKE 'pgroonga.%';

CREATE OPERATOR CLASS pgroonga_jsonb_full_text_search_ops_v2
	FOR TYPE jsonb
	USING pgroonga AS
		OPERATOR 12 &@ (jsonb, text),
		OPERATOR 28 &@~ (jsonb, text);

CREATE FUNCTION pgroonga_wal_apply()
	RETURNS bigint
	AS 'MODULE_PATHNAME', 'pgroonga_wal_apply_all'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga_wal_apply(indexName cstring)
	RETURNS bigint
	AS 'MODULE_PATHNAME', 'pgroonga_wal_apply_index'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga_wal_truncate()
	RETURNS bigint
	AS 'MODULE_PATHNAME', 'pgroonga_wal_truncate_all'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga_wal_truncate(indexName cstring)
	RETURNS bigint
	AS 'MODULE_PATHNAME', 'pgroonga_wal_truncate_index'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga_is_writable()
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_is_writable'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga_set_writable(newWritable bool)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_set_writable'
	LANGUAGE C
	VOLATILE
	STRICT;

CREATE FUNCTION pgroonga_normalize(target text)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_normalize'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga_normalize(target text, normalizerName text)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_normalize'
	LANGUAGE C
	IMMUTABLE
	STRICT;
