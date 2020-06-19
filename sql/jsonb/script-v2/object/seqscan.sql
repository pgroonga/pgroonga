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

SET enable_seqscan = on;
SET enable_indexscan = off;
SET enable_bitmapscan = off;

SELECT id, record
  FROM logs
 WHERE record &` 'paths @ ".message" && type == "object"'
 ORDER BY id;

DROP TABLE logs;
