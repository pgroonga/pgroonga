-- Downgrade SQL

/* v1 */
CREATE OPERATOR CLASS pgroonga.text_full_text_search_ops FOR TYPE text
    USING pgroonga AS
        OPERATOR 6 ~~,
        OPERATOR 7 ~~*,
        OPERATOR 8 %%,
        OPERATOR 9 @@,
        OPERATOR 12 &@,
        OPERATOR 13 &?, -- For backward compatibility
        OPERATOR 28 &@~;

CREATE OPERATOR CLASS pgroonga.text_array_full_text_search_ops FOR TYPE text[]
    USING pgroonga AS
        OPERATOR 8 %% (text[], text),
        OPERATOR 9 @@ (text[], text),
        OPERATOR 12 &@ (text[], text),
        OPERATOR 13 &? (text[], text), -- For backward compatibility
        OPERATOR 28 &@~ (text[], text);

CREATE OPERATOR CLASS pgroonga.varchar_full_text_search_ops FOR TYPE varchar
    USING pgroonga AS
        OPERATOR 8 %%,
        OPERATOR 9 @@,
        OPERATOR 12 &@,
        OPERATOR 13 &?, -- For backward compatibility
        OPERATOR 28 &@~;

CREATE OPERATOR CLASS pgroonga.varchar_ops FOR TYPE varchar
    USING pgroonga AS
        OPERATOR 1 < (text, text),
        OPERATOR 2 <= (text, text),
        OPERATOR 3 = (text, text),
        OPERATOR 4 >= (text, text),
        OPERATOR 5 > (text, text);

CREATE OPERATOR CLASS pgroonga.varchar_array_ops FOR TYPE varchar[]
    USING pgroonga AS
        OPERATOR 8 %% (varchar[], varchar),
        OPERATOR 23 &> (varchar[], varchar);

CREATE OPERATOR CLASS pgroonga.bool_ops FOR TYPE bool
    USING pgroonga AS
        OPERATOR 1 <,
        OPERATOR 2 <=,
        OPERATOR 3 =,
        OPERATOR 4 >=,
        OPERATOR 5 >;

CREATE OPERATOR CLASS pgroonga.int2_ops FOR TYPE int2
    USING pgroonga AS
        OPERATOR 1 <,
        OPERATOR 2 <=,
        OPERATOR 3 =,
        OPERATOR 4 >=,
        OPERATOR 5 >;

CREATE OPERATOR CLASS pgroonga.int4_ops FOR TYPE int4
    USING pgroonga AS
        OPERATOR 1 <,
        OPERATOR 2 <=,
        OPERATOR 3 =,
        OPERATOR 4 >=,
        OPERATOR 5 >;

CREATE OPERATOR CLASS pgroonga.int8_ops FOR TYPE int8
    USING pgroonga AS
        OPERATOR 1 <,
        OPERATOR 2 <=,
        OPERATOR 3 =,
        OPERATOR 4 >=,
        OPERATOR 5 >;

CREATE OPERATOR CLASS pgroonga.timestamp_ops FOR TYPE timestamp
    USING pgroonga AS
        OPERATOR 1 <,
        OPERATOR 2 <=,
        OPERATOR 3 =,
        OPERATOR 4 >=,
        OPERATOR 5 >;

CREATE OPERATOR CLASS pgroonga.timestamptz_ops FOR TYPE timestamptz
    USING pgroonga AS
        OPERATOR 1 <,
        OPERATOR 2 <=,
        OPERATOR 3 =,
        OPERATOR 4 >=,
        OPERATOR 5 >;

CREATE OPERATOR CLASS pgroonga.jsonb_ops FOR TYPE jsonb
    USING pgroonga AS
        OPERATOR 9 @@ (jsonb, text),
        OPERATOR 11 @>,
        OPERATOR 12 &@ (jsonb, text),
        OPERATOR 13 &? (jsonb, text), -- For backward compatibility
        OPERATOR 15 &` (jsonb, text),
        OPERATOR 28 &@~ (jsonb, text);

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

-- v2
CREATE OPERATOR CLASS pgroonga.text_full_text_search_ops_v2 FOR TYPE text
    USING pgroonga AS
        OPERATOR 6 ~~,
        OPERATOR 7 ~~*,
        OPERATOR 8 %%, -- For backward compatibility
        OPERATOR 9 @@, -- For backward compatibility
        OPERATOR 12 &@,
        OPERATOR 13 &?, -- For backward compatibility
        OPERATOR 14 &~?, -- For backward compatibility
        OPERATOR 15 &`,
        OPERATOR 18 &@| (text, text[]),
        OPERATOR 19 &?| (text, text[]), -- For backward compatibility
        OPERATOR 26 &@> (text, text[]), -- For backward compatibility
        OPERATOR 27 &?> (text, text[]), -- For backward compatibility
        OPERATOR 28 &@~,
        OPERATOR 29 &@*,
        OPERATOR 30 &@~| (text, text[]);

CREATE OPERATOR CLASS pgroonga.text_array_full_text_search_ops_v2 FOR TYPE text[]
    USING pgroonga AS
        OPERATOR 8 %% (text[], text), -- For backward compatibility
        OPERATOR 9 @@ (text[], text), -- For backward compatibility
        OPERATOR 12 &@ (text[], text),
        OPERATOR 13 &? (text[], text), -- For backward compatibility
        OPERATOR 14 &~? (text[], text), -- For backward compatibility
        OPERATOR 15 &` (text[], text),
        OPERATOR 18 &@| (text[], text[]),
        OPERATOR 19 &?| (text[], text[]), -- For backward compatibility
        OPERATOR 28 &@~ (text[], text),
        OPERATOR 29 &@* (text[], text),
        OPERATOR 30 &@~| (text[], text[]);

CREATE OPERATOR CLASS pgroonga.text_term_search_ops_v2 FOR TYPE text
        OPERATOR 1 <,
        OPERATOR 2 <=,
        OPERATOR 3 =,
        OPERATOR 4 >=,
        OPERATOR 5 >,
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

CREATE OPERATOR CLASS pgroonga.varchar_full_text_search_ops_v2 FOR TYPE varchar
    USING pgroonga AS
        OPERATOR 8 %%, -- For backward compatibility
        OPERATOR 9 @@, -- For backward compatibility
        OPERATOR 12 &@,
        OPERATOR 13 &?, -- For backward compatibility
        OPERATOR 14 &~?, -- For backward compatibility
        OPERATOR 15 &`,
        OPERATOR 18 &@| (text, text[]),
        OPERATOR 19 &?| (text, text[]), -- For backward compatibility
        OPERATOR 26 &@> (text, text[]), -- For backward compatibility
        OPERATOR 27 &?> (text, text[]), -- For backward compatibility
        OPERATOR 28 &@~,
        OPERATOR 29 &@*,
        OPERATOR 30 &@~| (text, text[]);

CREATE OPERATOR CLASS pgroonga.varchar_array_term_search_ops_v2
    FOR TYPE varchar[]
    USING pgroonga AS
        OPERATOR 8 %% (varchar[], varchar), -- For backward compatibility
        OPERATOR 23 &> (varchar[], varchar);

CREATE OPERATOR CLASS pgroonga.varchar_regexp_ops_v2 FOR TYPE varchar
    USING pgroonga AS
        OPERATOR 10 @~, -- For backward compatibility
        OPERATOR 22 &~;

CREATE FUNCTION pgroonga.escape(value float4)
    RETURNS text
    AS 'MODULE_PATHNAME', 'pgroonga_escape_float8'
    LANGUAGE C
    IMMUTABLE
    STRICT
    PARALLEL SAFE;

