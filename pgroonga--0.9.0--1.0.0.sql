-- Support index only scan
CREATE FUNCTION pgroonga.canreturn(internal)
	RETURNS internal
	AS 'MODULE_PATHNAME', 'pgroonga_canreturn'
	LANGUAGE C;

UPDATE pg_catalog.pg_am
   SET amcanreturn = 'pgroonga.canreturn'
 WHERE amname = 'pgroonga';


-- Support @~
UPDATE pg_catalog.pg_am SET amstrategies = 10 WHERE amname = 'pgroonga';

CREATE FUNCTION pgroonga.match_regexp(text, text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_regexp_text'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga.match_regexp(varchar, varchar)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_regexp_varchar'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE OPERATOR @~ (
	PROCEDURE = pgroonga.match_regexp,
	LEFTARG = text,
	RIGHTARG = text
);

CREATE OPERATOR @~ (
	PROCEDURE = pgroonga.match_regexp,
	LEFTARG = varchar,
	RIGHTARG = varchar
);

CREATE OPERATOR CLASS pgroonga.text_regexp_ops FOR TYPE text
	USING pgroonga AS
		OPERATOR 6 pg_catalog.~~,
		OPERATOR 10 @~;

CREATE OPERATOR CLASS pgroonga.varchar_regexp_ops FOR TYPE varchar
	USING pgroonga AS
		OPERATOR 10 @~;
