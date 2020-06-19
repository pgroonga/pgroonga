CREATE TABLE logs (
  id int,
  record jsonb
);

INSERT INTO logs VALUES (1, '
[
  ["web"],
  {"ipv4": ["127.0.0.1"]},
  {"code": 100, "content": "hello"}
]
');
INSERT INTO logs VALUES (2, '
[
  ["mail"],
  {"ipv4": ["127.0.0.2"]},
  "hello"
]
');
INSERT INTO logs VALUES (3, '
[
  [],
  {},
  ["hello", "world"]
]
');

CREATE INDEX pgroonga_index ON logs
  USING pgroonga (record pgroonga_jsonb_ops_v2);

SET enable_seqscan = off;
SET enable_indexscan = off;
SET enable_bitmapscan = on;

EXPLAIN (COSTS OFF)
SELECT id, record
  FROM logs
 WHERE record &` 'paths @ ".[][\"ipv4\"][]" && type == "array"'
 ORDER BY id;

SELECT id, record
  FROM logs
 WHERE record &` 'paths @ ".[][\"ipv4\"][]" && type == "array"'
 ORDER BY id;

DROP TABLE logs;
