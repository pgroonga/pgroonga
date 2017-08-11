CREATE TABLE synonyms (
  term text PRIMARY KEY,
  synonyms text[]
);

SELECT pgroonga_query_expand('synonyms', 'term', 'synonyms', 'Groonga');

DROP TABLE synonyms;
