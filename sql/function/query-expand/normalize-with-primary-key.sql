CREATE TABLE synonyms (
  term text PRIMARY KEY,
  synonyms text[]
);

CREATE INDEX synonyms_term_index ON synonyms
  USING pgroonga (term pgroonga.text_term_search_ops_v2);

INSERT INTO synonyms VALUES ('Groonga', ARRAY['Groonga', 'Senna']);
INSERT INTO synonyms VALUES ('GROONGA', ARRAY['"Full text search"']);

SELECT pgroonga.query_expand('synonyms', 'term', 'synonyms', 'groonga');

DROP TABLE synonyms;
