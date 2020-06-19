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

SET enable_seqscan = on;
SET enable_indexscan = off;
SET enable_bitmapscan = off;

SELECT id, record
  FROM logs
 WHERE record &` 'paths @ ".[][\"ipv4\"][]" && type == "array"'
 ORDER BY id;

DROP TABLE logs;
