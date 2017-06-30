CREATE TABLE synonyms (
  term text,
  synonyms text[]
);

INSERT INTO synonyms VALUES ('Groonga', ARRAY['Groonga', 'Senna']);

SELECT pgroonga.query_expand('synonyms', 'term', 'synonyms', 'Groonga');

DROP TABLE synonyms;
