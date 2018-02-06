CREATE TABLE memos (
  content text
);

CREATE INDEX pgrn_index ON memos
  USING pgroonga (content pgroonga_text_regexp_ops_v2)
  WITH (normalizer = 'none',
        regexp_search_normalizer = 'NormalizerNFKC51');

SELECT entry->>7 AS normalizer
  FROM jsonb_array_elements((pgroonga_command('table_list')::jsonb#>'{1}') - 0)
       AS entry
 WHERE entry->>1 = 'Lexicon' || 'pgrn_index'::regclass::oid || '_0';

DROP TABLE memos;
