CREATE TABLE synonyms (
  term text PRIMARY KEY,
  synonym text
);

SELECT pgroonga.query_expand('synonyms', 'term', 'synonym', 'Groonga');

DROP TABLE synonyms;
