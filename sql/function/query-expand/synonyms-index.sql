CREATE TABLE synonym_groups (
  synonyms text[]
);

CREATE INDEX synonym_groups_synonyms_index
  ON synonym_groups
  USING pgroonga (synonyms pgroonga_text_array_term_search_ops_v2);

INSERT INTO synonym_groups VALUES (ARRAY['Groonga', 'Senna']);

SELECT pgroonga_query_expand('synonym_groups',
                             'synonyms',
                             'synonyms',
                             'groonga');

DROP TABLE synonym_groups;
