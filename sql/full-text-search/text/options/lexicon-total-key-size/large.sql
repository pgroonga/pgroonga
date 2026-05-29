CREATE TABLE memos (
  content text
);

CREATE INDEX pgrn_index ON memos
  USING pgroonga (content)
  WITH (lexicon_total_key_size = 'large');

SELECT entry->>3 AS flags
  FROM json_array_elements(pgroonga_command('table_list')::json#>'{1}')
       AS entry
 WHERE entry->>1 = 'Lexicon' || 'pgrn_index'::regclass::oid || '_0';

DROP TABLE memos;
