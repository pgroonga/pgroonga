CREATE TABLE memos (
  content varchar(256)
);

INSERT INTO memos VALUES ('090-1234-5678');
INSERT INTO memos VALUES ('(090)1234-5678');

CREATE INDEX pgrn_index ON memos
 USING pgroonga (content pgroonga_varchar_full_text_search_ops_v2)
  WITH (tokenizer = 'TokenNgram("loose_symbol", true)');

SET enable_seqscan = off;
SET enable_indexscan = off;
SET enable_bitmapscan = on;

EXPLAIN (COSTS OFF)
SELECT content, pgroonga_score(tableoid, ctid)
  FROM memos
 WHERE content &@~
       ('090-12345678',
        ARRAY[5],
        'pgrn_index')::pgroonga_full_text_search_condition;

SELECT content, pgroonga_score(tableoid, ctid)
  FROM memos
 WHERE content &@~
       ('090-12345678',
        ARRAY[5],
        'pgrn_index')::pgroonga_full_text_search_condition;

DROP TABLE memos;
