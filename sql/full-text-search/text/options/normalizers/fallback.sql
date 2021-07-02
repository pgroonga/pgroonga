CREATE TABLE memos (
  content text
);

CREATE INDEX pgrn_index ON memos
  USING pgroonga (content)
  WITH (normalizers = 'NormalizerNFKC130,
                       NormalizerNFKC130');

SELECT entry->>7 AS normalizers
  FROM jsonb_array_elements((pgroonga_command('table_list')::jsonb#>'{1}') - 0)
       AS entry
 WHERE entry->>1 = 'Lexicon' || 'pgrn_index'::regclass::oid || '_0';

DROP TABLE memos;
