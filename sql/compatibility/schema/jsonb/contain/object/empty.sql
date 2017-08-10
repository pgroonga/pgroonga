CREATE TABLE logs (
  id int,
  record jsonb
);

INSERT INTO logs VALUES (1, '{}');
INSERT INTO logs VALUES (2, '{"key": 100}');
INSERT INTO logs VALUES (3, '{"key": "hello"}');
INSERT INTO logs VALUES (4, '{"key": true}');
INSERT INTO logs VALUES (5, '{"key": []}');
INSERT INTO logs VALUES (6, '[{}]');

CREATE INDEX pgroonga_index ON logs
  USING pgroonga (record pgroonga.jsonb_ops);

SET enable_seqscan = off;
SET enable_indexscan = on;
SET enable_bitmapscan = off;

SELECT id, record
  FROM logs
 WHERE record @> '{}'::jsonb
 ORDER BY id;

DROP TABLE logs;
