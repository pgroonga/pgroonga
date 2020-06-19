CREATE TABLE logs (
  id int,
  record jsonb
);

INSERT INTO logs VALUES (1, '
{
  "tags": ["web"],
  "host": {"ipv4": "127.0.0.1"},
  "message": {"code": 100, "content": "hello"}
}
');
INSERT INTO logs VALUES (2, '
{
  "tags": ["mail"],
  "host": {"ipv4": "127.0.0.2"},
  "message": "hello"
}
');
INSERT INTO logs VALUES (3, '
{
  "tags": [],
  "host": {},
  "message": ["hello", "world"]
}
');

CREATE INDEX pgroonga_index ON logs
  USING pgroonga (record pgroonga_jsonb_ops_v2);

SET enable_seqscan = off;
SET enable_indexscan = on;
SET enable_bitmapscan = off;

EXPLAIN (COSTS OFF)
SELECT id, record
  FROM logs
 WHERE record &` 'paths @ ".message" && type == "object"'
 ORDER BY id;

SELECT id, record
  FROM logs
 WHERE record &` 'paths @ ".message" && type == "object"'
 ORDER BY id;

DROP TABLE logs;
