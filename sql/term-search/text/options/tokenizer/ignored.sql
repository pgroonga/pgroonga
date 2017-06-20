CREATE TABLE tags (
  name text
);

CREATE INDEX pgrn_index ON tags
  USING pgroonga (name pgroonga.text_term_search_ops_v2)
  WITH (tokenizer = "TokenDelimit");

SELECT entry->>6 AS tokenizer
  FROM jsonb_array_elements((pgroonga.command('table_list')::jsonb#>'{1}') - 0)
       AS entry
 WHERE entry->>1 = 'Lexicon' || 'pgrn_index'::regclass::oid || '_0';

DROP TABLE tags;
