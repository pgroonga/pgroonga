SET search_path = public;

CREATE SCHEMA pgroonga;

CREATE FUNCTION pgroonga.query_expand(term text)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_query_expand'
	LANGUAGE C
	STABLE
	STRICT;

CREATE FUNCTION pgroonga.score("row" record)
	RETURNS float8
	AS 'MODULE_PATHNAME', 'pgroonga_score'
	LANGUAGE C
	VOLATILE
	STRICT;

CREATE FUNCTION pgroonga.table_name(indexName cstring)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_table_name'
	LANGUAGE C
	STABLE
	STRICT;

CREATE FUNCTION pgroonga.command(groongaCommand text)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_command'
	LANGUAGE C
	VOLATILE
	STRICT;

CREATE FUNCTION pgroonga.command(groongaCommand text, arguments text[])
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_command'
	LANGUAGE C
	VOLATILE
	STRICT;

CREATE FUNCTION pgroonga.snippet_html(target text, keywords text[])
	RETURNS text[]
	AS 'MODULE_PATHNAME', 'pgroonga_snippet_html'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga.highlight_html(target text, keywords text[])
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_highlight_html'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga.match_positions_byte(target text, keywords text[])
	RETURNS integer[2][]
	AS 'MODULE_PATHNAME', 'pgroonga_match_positions_byte'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga.match_positions_character(target text, keywords text[])
	RETURNS integer[2][]
	AS 'MODULE_PATHNAME', 'pgroonga_match_positions_character'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga.query_extract_keywords(query text)
	RETURNS text[]
	AS 'MODULE_PATHNAME', 'pgroonga_query_extract_keywords'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga.flush(indexName cstring)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_flush'
	LANGUAGE C
	VOLATILE
	STRICT;

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


/* v1 */
CREATE FUNCTION pgroonga.match_term(target text, term text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_term_text'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga.match_term(target text[], term text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_term_text_array'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga.match_term(target varchar, term varchar)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_term_varchar'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga.match_term(target varchar[], term varchar)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_term_varchar_array'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE OPERATOR %% (
	PROCEDURE = pgroonga.match_term,
	LEFTARG = text,
	RIGHTARG = text
);

CREATE OPERATOR %% (
	PROCEDURE = pgroonga.match_term,
	LEFTARG = text[],
	RIGHTARG = text
);

CREATE OPERATOR %% (
	PROCEDURE = pgroonga.match_term,
	LEFTARG = varchar,
	RIGHTARG = varchar
);

CREATE OPERATOR %% (
	PROCEDURE = pgroonga.match_term,
	LEFTARG = varchar[],
	RIGHTARG = varchar
);


CREATE FUNCTION pgroonga.match_query(text, text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_query_text'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga.match_query(text[], text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_query_text_array'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga.match_query(varchar, varchar)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_query_varchar'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE OPERATOR @@ (
	PROCEDURE = pgroonga.match_query,
	LEFTARG = text,
	RIGHTARG = text
);

CREATE OPERATOR @@ (
	PROCEDURE = pgroonga.match_query,
	LEFTARG = text[],
	RIGHTARG = text
);

CREATE OPERATOR @@ (
	PROCEDURE = pgroonga.match_query,
	LEFTARG = varchar,
	RIGHTARG = varchar
);


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


/* v2 */
CREATE FUNCTION pgroonga.match_text(text, text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_text'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE OPERATOR &@ (
	PROCEDURE = pgroonga.match_text,
	LEFTARG = text,
	RIGHTARG = text
);

CREATE FUNCTION pgroonga.match_text_array(text[], text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_text_array'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE OPERATOR &@ (
	PROCEDURE = pgroonga.match_text_array,
	LEFTARG = text[],
	RIGHTARG = text
);

CREATE FUNCTION pgroonga.match_varchar(varchar, varchar)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_varchar'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE OPERATOR &@ (
	PROCEDURE = pgroonga.match_varchar,
	LEFTARG = varchar,
	RIGHTARG = varchar
);

CREATE FUNCTION pgroonga.contain_varchar_array(varchar[], varchar)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_contain_varchar_array'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE OPERATOR &> (
	PROCEDURE = pgroonga.contain_varchar_array,
	LEFTARG = varchar[],
	RIGHTARG = varchar
);

DO LANGUAGE plpgsql $$
BEGIN
	PERFORM 1
		FROM pg_type
		WHERE typname = 'jsonb';

	IF FOUND THEN
		CREATE FUNCTION pgroonga.match_jsonb(jsonb, text)
			RETURNS bool
			AS 'MODULE_PATHNAME', 'pgroonga_match_jsonb'
			LANGUAGE C
			IMMUTABLE
			STRICT;

		CREATE OPERATOR &@ (
			PROCEDURE = pgroonga.match_jsonb,
			LEFTARG = jsonb,
			RIGHTARG = text
		);
	END IF;
END;
$$;

CREATE FUNCTION pgroonga.query_text(text, text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_query_text'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE OPERATOR &? (
	PROCEDURE = pgroonga.query_text,
	LEFTARG = text,
	RIGHTARG = text
);

CREATE FUNCTION pgroonga.query_text_array(text[], text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_query_text_array'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE OPERATOR &? (
	PROCEDURE = pgroonga.query_text_array,
	LEFTARG = text[],
	RIGHTARG = text
);

CREATE FUNCTION pgroonga.query_varchar(varchar, varchar)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_query_varchar'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE OPERATOR &? (
	PROCEDURE = pgroonga.query_varchar,
	LEFTARG = varchar,
	RIGHTARG = varchar
);

DO LANGUAGE plpgsql $$
BEGIN
	PERFORM 1
		FROM pg_type
		WHERE typname = 'jsonb';

	IF FOUND THEN
		CREATE FUNCTION pgroonga.query_jsonb(jsonb, text)
			RETURNS bool
			AS 'MODULE_PATHNAME', 'pgroonga_query_jsonb'
			LANGUAGE C
			IMMUTABLE
			STRICT;

		CREATE OPERATOR &? (
			PROCEDURE = pgroonga.query_jsonb,
			LEFTARG = jsonb,
			RIGHTARG = text
		);
	END IF;
END;
$$;

CREATE FUNCTION pgroonga.similar_text(text, text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_similar_text'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE OPERATOR &~? (
	PROCEDURE = pgroonga.similar_text,
	LEFTARG = text,
	RIGHTARG = text
);

CREATE FUNCTION pgroonga.similar_text_array(text[], text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_similar_text_array'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE OPERATOR &~? (
	PROCEDURE = pgroonga.similar_text_array,
	LEFTARG = text[],
	RIGHTARG = text
);

CREATE FUNCTION pgroonga.similar_varchar(varchar, varchar)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_similar_varchar'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE OPERATOR &~? (
	PROCEDURE = pgroonga.similar_varchar,
	LEFTARG = varchar,
	RIGHTARG = varchar
);

CREATE FUNCTION pgroonga.prefix_text(text, text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_prefix_text'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE OPERATOR &^ (
	PROCEDURE = pgroonga.prefix_text,
	LEFTARG = text,
	RIGHTARG = text
);

CREATE FUNCTION pgroonga.prefix_text_array(text[], text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_prefix_text_array'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE OPERATOR &^ (
	PROCEDURE = pgroonga.prefix_text_array,
	LEFTARG = text[],
	RIGHTARG = text
);

/* Deprecated since 1.2.1. */
CREATE OPERATOR &^> (
	PROCEDURE = pgroonga.prefix_text_array,
	LEFTARG = text[],
	RIGHTARG = text
);

CREATE FUNCTION pgroonga.prefix_rk_text(text, text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_prefix_rk_text'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE OPERATOR &^~ (
	PROCEDURE = pgroonga.prefix_rk_text,
	LEFTARG = text,
	RIGHTARG = text
);

CREATE FUNCTION pgroonga.prefix_rk_text_array(text[], text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_prefix_rk_text_array'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE OPERATOR &^~ (
	PROCEDURE = pgroonga.prefix_rk_text_array,
	LEFTARG = text[],
	RIGHTARG = text
);

/* Deprecated since 1.2.1. */
CREATE OPERATOR &^~> (
	PROCEDURE = pgroonga.prefix_rk_text_array,
	LEFTARG = text[],
	RIGHTARG = text
);

CREATE FUNCTION pgroonga.script_text(text, text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_script_text'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE OPERATOR &` (
	PROCEDURE = pgroonga.script_text,
	LEFTARG = text,
	RIGHTARG = text
);

CREATE FUNCTION pgroonga.script_text_array(text[], text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_script_text_array'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE OPERATOR &` (
	PROCEDURE = pgroonga.script_text_array,
	LEFTARG = text[],
	RIGHTARG = text
);

CREATE FUNCTION pgroonga.script_varchar(varchar, varchar)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_script_varchar'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE OPERATOR &` (
	PROCEDURE = pgroonga.script_varchar,
	LEFTARG = varchar,
	RIGHTARG = varchar
);

DO LANGUAGE plpgsql $$
BEGIN
	PERFORM 1
		FROM pg_type
		WHERE typname = 'jsonb';

	IF FOUND THEN
		CREATE FUNCTION pgroonga.script_jsonb(jsonb, text)
			RETURNS bool
			AS 'MODULE_PATHNAME', 'pgroonga_script_jsonb'
			LANGUAGE C
			IMMUTABLE
			STRICT;

		CREATE OPERATOR &` (
			PROCEDURE = pgroonga.script_jsonb,
			LEFTARG = jsonb,
			RIGHTARG = text
		);
	END IF;
END;
$$;

CREATE FUNCTION pgroonga.match_in_text(text, text[])
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_in_text'
	LANGUAGE C
	IMMUTABLE
	STRICT;

/* Deprecated since 1.2.1. */
CREATE OPERATOR &@> (
	PROCEDURE = pgroonga.match_in_text,
	LEFTARG = text,
	RIGHTARG = text[]
);

CREATE OPERATOR &@| (
	PROCEDURE = pgroonga.match_in_text,
	LEFTARG = text,
	RIGHTARG = text[]
);

CREATE FUNCTION pgroonga.match_in_text_array(text[], text[])
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_in_text_array'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE OPERATOR &@| (
	PROCEDURE = pgroonga.match_in_text_array,
	LEFTARG = text[],
	RIGHTARG = text[]
);

CREATE FUNCTION pgroonga.match_in_varchar(varchar, varchar[])
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_in_varchar'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE OPERATOR &@| (
	PROCEDURE = pgroonga.match_in_varchar,
	LEFTARG = varchar,
	RIGHTARG = varchar[]
);

CREATE FUNCTION pgroonga.query_in_text(text, text[])
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_query_in_text'
	LANGUAGE C
	IMMUTABLE
	STRICT;

/* Deprecated since 1.2.1. */
CREATE OPERATOR &?> (
	PROCEDURE = pgroonga.query_in_text,
	LEFTARG = text,
	RIGHTARG = text[]
);

CREATE OPERATOR &?| (
	PROCEDURE = pgroonga.query_in_text,
	LEFTARG = text,
	RIGHTARG = text[]
);

CREATE FUNCTION pgroonga.query_in_text_array(text[], text[])
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_query_in_text_array'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE OPERATOR &?| (
	PROCEDURE = pgroonga.query_in_text_array,
	LEFTARG = text[],
	RIGHTARG = text[]
);

CREATE FUNCTION pgroonga.query_in_varchar(varchar, varchar[])
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_query_in_varchar'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE OPERATOR &?| (
	PROCEDURE = pgroonga.query_in_varchar,
	LEFTARG = varchar,
	RIGHTARG = varchar[]
);

CREATE FUNCTION pgroonga.prefix_in_text(text, text[])
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_prefix_in_text'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE OPERATOR &^| (
	PROCEDURE = pgroonga.prefix_in_text,
	LEFTARG = text,
	RIGHTARG = text[]
);

CREATE FUNCTION pgroonga.prefix_in_text_array(text[], text[])
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_prefix_in_text_array'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE OPERATOR &^| (
	PROCEDURE = pgroonga.prefix_in_text_array,
	LEFTARG = text[],
	RIGHTARG = text[]
);

CREATE FUNCTION pgroonga.prefix_rk_in_text(text, text[])
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_prefix_rk_in_text'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE OPERATOR &^~| (
	PROCEDURE = pgroonga.prefix_rk_in_text,
	LEFTARG = text,
	RIGHTARG = text[]
);

CREATE FUNCTION pgroonga.prefix_rk_in_text_array(text[], text[])
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_prefix_rk_in_text_array'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE OPERATOR &^~| (
	PROCEDURE = pgroonga.prefix_rk_in_text_array,
	LEFTARG = text[],
	RIGHTARG = text[]
);

CREATE FUNCTION pgroonga.regexp_text(text, text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_regexp_text'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE OPERATOR &~ (
	PROCEDURE = pgroonga.regexp_text,
	LEFTARG = text,
	RIGHTARG = text
);

CREATE FUNCTION pgroonga.regexp_varchar(varchar, varchar)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_regexp_varchar'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE OPERATOR &~ (
	PROCEDURE = pgroonga.regexp_varchar,
	LEFTARG = varchar,
	RIGHTARG = varchar
);

DO LANGUAGE plpgsql $$
BEGIN
	EXECUTE 'DROP ACCESS METHOD IF EXISTS pgroonga CASCADE';
	CREATE FUNCTION pgroonga.handler(internal)
		RETURNS index_am_handler
		AS 'MODULE_PATHNAME', 'pgroonga_handler'
		LANGUAGE C;
	EXECUTE 'CREATE ACCESS METHOD pgroonga ' ||
		'TYPE INDEX ' ||
		'HANDLER pgroonga.handler';
EXCEPTION
	WHEN syntax_error THEN
		CREATE FUNCTION pgroonga.insert(internal)
			RETURNS bool
			AS 'MODULE_PATHNAME', 'pgroonga_insert'
			LANGUAGE C;
		CREATE FUNCTION pgroonga.beginscan(internal)
			RETURNS internal
			AS 'MODULE_PATHNAME', 'pgroonga_beginscan'
			LANGUAGE C;
		CREATE FUNCTION pgroonga.gettuple(internal)
			RETURNS bool
			AS 'MODULE_PATHNAME', 'pgroonga_gettuple'
			LANGUAGE C;
		CREATE FUNCTION pgroonga.getbitmap(internal)
			RETURNS bigint
			AS 'MODULE_PATHNAME', 'pgroonga_getbitmap'
			LANGUAGE C;
		CREATE FUNCTION pgroonga.rescan(internal)
			RETURNS void
			AS 'MODULE_PATHNAME', 'pgroonga_rescan'
			LANGUAGE C;
		CREATE FUNCTION pgroonga.endscan(internal)
			RETURNS void
			AS 'MODULE_PATHNAME', 'pgroonga_endscan'
			LANGUAGE C;
		CREATE FUNCTION pgroonga.build(internal)
			RETURNS internal
			AS 'MODULE_PATHNAME', 'pgroonga_build'
			LANGUAGE C;
		CREATE FUNCTION pgroonga.buildempty(internal)
			RETURNS internal
			AS 'MODULE_PATHNAME', 'pgroonga_buildempty'
			LANGUAGE C;
		CREATE FUNCTION pgroonga.bulkdelete(internal)
			RETURNS internal
			AS 'MODULE_PATHNAME', 'pgroonga_bulkdelete'
			LANGUAGE C;
		CREATE FUNCTION pgroonga.vacuumcleanup(internal)
			RETURNS internal
			AS 'MODULE_PATHNAME', 'pgroonga_vacuumcleanup'
			LANGUAGE C;
		CREATE FUNCTION pgroonga.canreturn(internal)
			RETURNS internal
			AS 'MODULE_PATHNAME', 'pgroonga_canreturn'
			LANGUAGE C;
		CREATE FUNCTION pgroonga.costestimate(internal)
			RETURNS internal
			AS 'MODULE_PATHNAME', 'pgroonga_costestimate'
			LANGUAGE C;
		CREATE FUNCTION pgroonga.options(internal)
			RETURNS internal
			AS 'MODULE_PATHNAME', 'pgroonga_options'
			LANGUAGE C;

		DELETE FROM pg_am WHERE amname = 'pgroonga';
		INSERT INTO pg_am VALUES(
			'pgroonga',	-- amname
			27,		-- amstrategies
			0,		-- amsupport
			true,		-- amcanorder
			true,		-- amcanorderbyop
			true,		-- amcanbackward
			true,		-- amcanunique
			true,		-- amcanmulticol
			true,		-- amoptionalkey
			true,		-- amsearcharray
			false,		-- amsearchnulls
			false,		-- amstorage
			true,		-- amclusterable
			false,		-- ampredlocks
			0,		-- amkeytype
			'pgroonga.insert',	-- aminsert
			'pgroonga.beginscan',	-- ambeginscan
			'pgroonga.gettuple',	-- amgettuple
			'pgroonga.getbitmap',	-- amgetbitmap
			'pgroonga.rescan',	-- amrescan
			'pgroonga.endscan',	-- amendscan
			0,		-- ammarkpos,
			0,		-- amrestrpos,
			'pgroonga.build',	-- ambuild
			'pgroonga.buildempty',	-- ambuildempty
			'pgroonga.bulkdelete',	-- ambulkdelete
			'pgroonga.vacuumcleanup',	-- amvacuumcleanup
			'pgroonga.canreturn',		-- amcanreturn
			'pgroonga.costestimate',	-- amcostestimate
			'pgroonga.options'	-- amoptions
		);
END;
$$;


/* v1 */
CREATE OPERATOR CLASS pgroonga.text_full_text_search_ops DEFAULT FOR TYPE text
	USING pgroonga AS
		OPERATOR 6 ~~,
		OPERATOR 7 ~~*,
		OPERATOR 8 %%,
		OPERATOR 9 @@,
		OPERATOR 12 &@,
		OPERATOR 13 &?;

CREATE OPERATOR CLASS pgroonga.text_array_full_text_search_ops
	DEFAULT
	FOR TYPE text[]
	USING pgroonga AS
		OPERATOR 8 %% (text[], text),
		OPERATOR 9 @@ (text[], text),
		OPERATOR 12 &@ (text[], text),
		OPERATOR 13 &? (text[], text);

CREATE OPERATOR CLASS pgroonga.varchar_full_text_search_ops FOR TYPE varchar
	USING pgroonga AS
		OPERATOR 8 %%,
		OPERATOR 9 @@,
		OPERATOR 12 &@,
		OPERATOR 13 &?;

CREATE OPERATOR CLASS pgroonga.varchar_ops DEFAULT FOR TYPE varchar
	USING pgroonga AS
		OPERATOR 1 < (text, text),
		OPERATOR 2 <= (text, text),
		OPERATOR 3 = (text, text),
		OPERATOR 4 >= (text, text),
		OPERATOR 5 > (text, text);

CREATE OPERATOR CLASS pgroonga.varchar_array_ops
	DEFAULT
	FOR TYPE varchar[]
	USING pgroonga AS
		OPERATOR 8 %% (varchar[], varchar),
		OPERATOR 23 &> (varchar[], varchar);

CREATE OPERATOR CLASS pgroonga.bool_ops DEFAULT FOR TYPE bool
	USING pgroonga AS
		OPERATOR 1 <,
		OPERATOR 2 <=,
		OPERATOR 3 =,
		OPERATOR 4 >=,
		OPERATOR 5 >;

CREATE OPERATOR CLASS pgroonga.int2_ops DEFAULT FOR TYPE int2
	USING pgroonga AS
		OPERATOR 1 <,
		OPERATOR 2 <=,
		OPERATOR 3 =,
		OPERATOR 4 >=,
		OPERATOR 5 >;

CREATE OPERATOR CLASS pgroonga.int4_ops DEFAULT FOR TYPE int4
	USING pgroonga AS
		OPERATOR 1 <,
		OPERATOR 2 <=,
		OPERATOR 3 =,
		OPERATOR 4 >=,
		OPERATOR 5 >;

CREATE OPERATOR CLASS pgroonga.int8_ops DEFAULT FOR TYPE int8
	USING pgroonga AS
		OPERATOR 1 <,
		OPERATOR 2 <=,
		OPERATOR 3 =,
		OPERATOR 4 >=,
		OPERATOR 5 >;

CREATE OPERATOR CLASS pgroonga.float4_ops DEFAULT FOR TYPE float4
	USING pgroonga AS
		OPERATOR 1 <,
		OPERATOR 2 <=,
		OPERATOR 3 =,
		OPERATOR 4 >=,
		OPERATOR 5 >;

CREATE OPERATOR CLASS pgroonga.float8_ops DEFAULT FOR TYPE float8
	USING pgroonga AS
		OPERATOR 1 <,
		OPERATOR 2 <=,
		OPERATOR 3 =,
		OPERATOR 4 >=,
		OPERATOR 5 >;

CREATE OPERATOR CLASS pgroonga.timestamp_ops DEFAULT FOR TYPE timestamp
	USING pgroonga AS
		OPERATOR 1 <,
		OPERATOR 2 <=,
		OPERATOR 3 =,
		OPERATOR 4 >=,
		OPERATOR 5 >;

CREATE OPERATOR CLASS pgroonga.timestamptz_ops DEFAULT FOR TYPE timestamptz
	USING pgroonga AS
		OPERATOR 1 <,
		OPERATOR 2 <=,
		OPERATOR 3 =,
		OPERATOR 4 >=,
		OPERATOR 5 >;

DO LANGUAGE plpgsql $$
BEGIN
	PERFORM 1
		FROM pg_type
		WHERE typname = 'jsonb';

	IF FOUND THEN
		CREATE FUNCTION pgroonga.match_script_jsonb(jsonb, text)
			RETURNS bool
			AS 'MODULE_PATHNAME', 'pgroonga_match_script_jsonb'
			LANGUAGE C
			IMMUTABLE
			STRICT;

		CREATE OPERATOR @@ (
			PROCEDURE = pgroonga.match_script_jsonb,
			LEFTARG = jsonb,
			RIGHTARG = text
		);

		CREATE OPERATOR CLASS pgroonga.jsonb_ops DEFAULT FOR TYPE jsonb
			USING pgroonga AS
				OPERATOR 9 @@ (jsonb, text),
				OPERATOR 11 @>,
				OPERATOR 12 &@ (jsonb, text),
				OPERATOR 13 &? (jsonb, text),
				OPERATOR 15 &` (jsonb, text);
	END IF;
END;
$$;

CREATE OPERATOR CLASS pgroonga.text_regexp_ops FOR TYPE text
	USING pgroonga AS
		OPERATOR 6 ~~,
		OPERATOR 7 ~~*,
		OPERATOR 10 @~,
		OPERATOR 22 &~;

CREATE OPERATOR CLASS pgroonga.varchar_regexp_ops FOR TYPE varchar
	USING pgroonga AS
		OPERATOR 10 @~,
		OPERATOR 22 &~;

/* v2 */
CREATE OPERATOR CLASS pgroonga.text_full_text_search_ops_v2 FOR TYPE text
	USING pgroonga AS
		OPERATOR 6 ~~,
		OPERATOR 7 ~~*,
		OPERATOR 8 %%, -- For backward compatibility
		OPERATOR 9 @@, -- For backward compatibility
		OPERATOR 12 &@,
		OPERATOR 13 &?,
		OPERATOR 14 &~?,
		OPERATOR 15 &`,
		OPERATOR 18 &@| (text, text[]),
		OPERATOR 19 &?| (text, text[]),
		OPERATOR 26 &@> (text, text[]), -- For backward compatibility
		OPERATOR 27 &?> (text, text[]); -- For backward compatibility

CREATE OPERATOR CLASS pgroonga.text_array_full_text_search_ops_v2
	FOR TYPE text[]
	USING pgroonga AS
		OPERATOR 8 %% (text[], text), -- For backward compatibility
		OPERATOR 9 @@ (text[], text), -- For backward compatibility
		OPERATOR 12 &@ (text[], text),
		OPERATOR 13 &? (text[], text),
		OPERATOR 14 &~? (text[], text),
		OPERATOR 15 &` (text[], text),
		OPERATOR 18 &@| (text[], text[]),
		OPERATOR 19 &?| (text[], text[]);

CREATE OPERATOR CLASS pgroonga.text_term_search_ops_v2 FOR TYPE text
	USING pgroonga AS
		OPERATOR 16 &^,
		OPERATOR 17 &^~,
		OPERATOR 20 &^| (text, text[]),
		OPERATOR 21 &^~| (text, text[]);

CREATE OPERATOR CLASS pgroonga.text_array_term_search_ops_v2 FOR TYPE text[]
	USING pgroonga AS
		OPERATOR 16 &^ (text[], text),
		OPERATOR 17 &^~ (text[], text),
		OPERATOR 20 &^| (text[], text[]),
		OPERATOR 21 &^~| (text[], text[]),
		OPERATOR 24 &^> (text[], text), -- For backward compatibility
		OPERATOR 25 &^~> (text[], text); -- For backward compatibility

CREATE OPERATOR CLASS pgroonga.text_regexp_ops_v2 FOR TYPE text
	USING pgroonga AS
		OPERATOR 6 ~~,
		OPERATOR 7 ~~*,
		OPERATOR 10 @~, -- For backward compatibility
		OPERATOR 22 &~;

CREATE OPERATOR CLASS pgroonga.varchar_full_text_search_ops_v2
	FOR TYPE varchar
	USING pgroonga AS
		OPERATOR 8 %%, -- For backward compatibility
		OPERATOR 9 @@, -- For backward compatibility
		OPERATOR 12 &@,
		OPERATOR 13 &?,
		OPERATOR 14 &~?,
		OPERATOR 15 &`,
		OPERATOR 18 &@| (varchar, varchar[]),
		OPERATOR 19 &?| (varchar, varchar[]);

CREATE OPERATOR CLASS pgroonga.varchar_array_term_search_ops_v2
	FOR TYPE varchar[]
	USING pgroonga AS
		OPERATOR 8 %% (varchar[], varchar), -- For backward compatibility
		OPERATOR 23 &> (varchar[], varchar);

CREATE OPERATOR CLASS pgroonga.varchar_regexp_ops_v2 FOR TYPE varchar
	USING pgroonga AS
		OPERATOR 10 @~, -- For backward compatibility
		OPERATOR 22 &~;

DO LANGUAGE plpgsql $$
BEGIN
	PERFORM 1
		FROM pg_type
		WHERE typname = 'jsonb';

	IF FOUND THEN
		CREATE OPERATOR CLASS pgroonga.jsonb_ops_v2
			FOR TYPE jsonb
			USING pgroonga AS
				OPERATOR 9 @@ (jsonb, text), -- For backward compatibility
				OPERATOR 11 @>,
				OPERATOR 12 &@ (jsonb, text),
				OPERATOR 13 &? (jsonb, text),
				OPERATOR 15 &` (jsonb, text);
	END IF;
END;
$$;
