CREATE TABLE synonyms (
  term text PRIMARY KEY,
  synonyms text[]
);

INSERT INTO synonyms VALUES ('Mroonga', ARRAY['Mroonga', 'Groonga MySQL']);
INSERT INTO synonyms VALUES ('PGroonga', ARRAY['PGroonga', 'Groonga PostgreSQL']);

SELECT pgroonga.query_expand('synonyms', 'term', 'synonyms',
                             'Ruby (Mroonga OR PGroonga)');

DROP TABLE synonyms;
