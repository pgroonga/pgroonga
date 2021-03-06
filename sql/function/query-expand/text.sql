CREATE TABLE synonyms (
  term text,
  synonym text
);

CREATE INDEX synonyms_term_index ON synonyms (term);

DO LANGUAGE plpgsql $$
BEGIN
	PERFORM 1
		WHERE current_setting('server_version_num')::int >= 120000;
	IF FOUND THEN
		INSERT INTO synonyms VALUES ('PGroonga', 'PGroonga');
		INSERT INTO synonyms VALUES ('PGroonga', 'Groonga PostgreSQL');
	ELSE
		INSERT INTO synonyms VALUES ('PGroonga', 'Groonga PostgreSQL');
		INSERT INTO synonyms VALUES ('PGroonga', 'PGroonga');
	END IF;
END;
$$;

SELECT pgroonga_query_expand('synonyms', 'term', 'synonym', 'PGroonga');

DROP TABLE synonyms;
