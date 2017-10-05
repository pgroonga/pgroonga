CREATE TABLE synonyms (
  term text PRIMARY KEY,
  synonym int
);

SELECT pgroonga_query_expand('synonyms', 'term', 'synonym', 'PGroonga');

DROP TABLE synonyms;
