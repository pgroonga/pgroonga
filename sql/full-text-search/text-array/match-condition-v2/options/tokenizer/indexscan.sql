CREATE TABLE memos (
  title text,
  content text
);

INSERT INTO memos VALUES ('hyphen', '090-1234-5678');
INSERT INTO memos VALUES ('parenthesis', '(090)1234-5678');

CREATE INDEX pgrn_index ON memos
 USING pgroonga ((ARRAY[title, content])
                 pgroonga_text_array_full_text_search_ops_v2)
  WITH (tokenizer = 'TokenNgram("loose_symbol", true)');

SET enable_seqscan = off;
SET enable_indexscan = on;
SET enable_bitmapscan = off;

\pset format unaligned
EXPLAIN (COSTS OFF)
SELECT content, pgroonga_score(tableoid, ctid)
  FROM memos
 WHERE ARRAY[title, content] &@
         pgroonga_condition(
           '090-12345678',
           ARRAY[5, 2],
           index_name => 'pgrn_index'
         )
\g |sed -r -e "s/('.+'|ROW.+)::pgroonga/pgroonga/g"
\pset format aligned

SELECT content, pgroonga_score(tableoid, ctid)
  FROM memos
 WHERE ARRAY[title, content] &@
         pgroonga_condition(
           '090-12345678',
           ARRAY[5, 2],
           index_name => 'pgrn_index'
         );

DROP TABLE memos;
