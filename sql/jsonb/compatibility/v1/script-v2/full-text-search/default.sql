CREATE TABLE logs (
  id int,
  record jsonb
);

INSERT INTO logs VALUES (1, '{"message": "Hello World"}');
INSERT INTO logs VALUES (2, '{"message": "This is a pen"}');
INSERT INTO logs VALUES (3, '{"message": "Good-by World"}');

CREATE INDEX pgroonga_index ON logs
  USING pgroonga (record pgroonga_jsonb_ops);

SET enable_seqscan = off;
SET enable_indexscan = on;
SET enable_bitmapscan = off;

SELECT id, record
  FROM logs
 WHERE record &` 'string @ "world"'
 ORDER BY id;

DROP TABLE logs;
