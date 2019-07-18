CREATE TABLE synonym_groups (
  synonyms text[]
);

INSERT INTO synonym_groups VALUES (ARRAY['Groonga', 'Senna']);

SELECT pgroonga_query_expand('synonym_groups',
                             'synonyms',
                             'synonyms',
                             'Groonga');

DROP TABLE synonym_groups;
