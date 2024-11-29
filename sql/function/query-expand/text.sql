CREATE TABLE synonyms (
  term text,
  synonym text
);

CREATE INDEX synonyms_term_index ON synonyms (term);

INSERT INTO synonyms VALUES ('PGroonga', 'PGroonga');
INSERT INTO synonyms VALUES ('PGroonga', 'Groonga PostgreSQL');

SELECT pgroonga_query_expand('synonyms', 'term', 'synonym', 'PGroonga');

DROP TABLE synonyms;
