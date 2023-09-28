CREATE TABLE memos (
  title text,
  content text
);

INSERT INTO memos VALUES ('title1', 'Hello World');
INSERT INTO memos VALUES ('title2', 'He11o Wor1d');
INSERT INTO memos VALUES ('title3', 'Hello 110');

CREATE INDEX pgrn_index ON memos
 USING pgroonga (title pgroonga_text_full_text_search_ops_v2,
                 content pgroonga_text_full_text_search_ops_v2)
  WITH (tokenizer_mapping = '{
          "content": "TokenDelimit"
        }');

SET enable_seqscan = off;
SET enable_indexscan = off;
SET enable_bitmapscan = on;

\pset format unaligned
EXPLAIN (COSTS OFF)
SELECT content, pgroonga_score(tableoid, ctid)
  FROM memos
 WHERE content &@~
       ('11',
        null,
        'pgrn_index.content')::pgroonga_full_text_search_condition
\g |sed -r -e "s/('.+'|ROW.+)::pgroonga/pgroonga/g"
\pset format aligned

SELECT content, pgroonga_score(tableoid, ctid)
  FROM memos
 WHERE content &@~
       ('11',
        null,
        'pgrn_index.content')::pgroonga_full_text_search_condition;

DROP TABLE memos;
