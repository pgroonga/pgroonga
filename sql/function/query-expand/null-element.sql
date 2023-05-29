CREATE TABLE synonyms (
  term text PRIMARY KEY,
  synonyms text[]
);

INSERT INTO synonyms VALUES ('Groonga', ARRAY['Groonga', NULL, 'Senna']);

SELECT pgroonga_query_expand('synonyms', 'term', 'synonyms', 'Groonga');

DROP TABLE synonyms;
