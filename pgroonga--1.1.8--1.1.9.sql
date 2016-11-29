CREATE FUNCTION pgroonga.command_escape_value(value text)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_command_escape_value'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga.query_escape(query text)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_query_escape'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga.escape(value text)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_escape_string'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga.escape(value text, special_characters text)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_escape_string'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga.escape(value boolean)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_escape_boolean'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga.escape(value int2)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_escape_int2'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga.escape(value int4)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_escape_int4'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga.escape(value int8)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_escape_int8'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga.escape(value float4)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_escape_float8'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga.escape(value float8)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_escape_float8'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga.escape(value timestamp)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_escape_timestamptz'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga.escape(value timestamptz)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_escape_timestamptz'
	LANGUAGE C
	IMMUTABLE
	STRICT;
