CREATE TABLE synonyms (
  term text PRIMARY KEY,
  synonyms text[]
);

SELECT pgroonga.query_expand('synonyms', 'term', 'synonyms', 'Groonga');
