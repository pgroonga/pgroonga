-- Downgrade SQL

DO LANGUAGE plpgsql $$
BEGIN
	PERFORM setting FROM pg_settings
	 WHERE name = 'server_version' AND
	       setting LIKE '%Postgres-XL%';
	-- "CREATE SCHEMA pgroonga" and CREATE FUNCTION pgroonga.XXX" don't work
	-- on Postgres-XL.
	IF NOT FOUND THEN
		CREATE SCHEMA pgroonga;

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
	END IF;
END;
$$;
