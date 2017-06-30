CREATE TABLE synonyms (
  term text,
  synonyms text[]
);

CREATE INDEX synonyms_term_index ON synonyms (term);

INSERT INTO synonyms VALUES ('Groonga', ARRAY['Groonga', 'Senna']);
INSERT INTO synonyms VALUES ('Groonga', ARRAY['Mroonga', 'PGroonga', 'Rroonga']);

SELECT pgroonga.query_expand('synonyms', 'term', 'synonyms', 'Groonga');

DROP TABLE synonyms;
