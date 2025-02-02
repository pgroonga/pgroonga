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
