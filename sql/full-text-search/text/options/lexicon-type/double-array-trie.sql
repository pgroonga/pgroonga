CREATE TABLE memos (
  content text
);

INSERT INTO memos VALUES ('123');

CREATE INDEX pgrn_index ON memos
  USING pgroonga (content)
  WITH (lexicon_type = 'double_array_trie');

SET enable_seqscan = off;

SELECT *
  FROM memos
 WHERE content &@ '1';

SELECT entry->>3 AS flags
  FROM json_array_elements(pgroonga_command('table_list')::json#>'{1}')
       AS entry
 WHERE entry->>1 = 'Lexicon' || 'pgrn_index'::regclass::oid || '_0';

DROP TABLE memos;
