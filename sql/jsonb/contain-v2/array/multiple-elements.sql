CREATE TABLE logs (
  id int,
  record jsonb
);

INSERT INTO logs
     VALUES (1, '{"body": {"values": [100, "Hello", true]}}');
INSERT INTO logs
     VALUES (2, '{"values": [100, "Hello", true]}');
INSERT INTO logs
     VALUES (3, '{"body": {"values": [100, "Hello", true, "World"]}}');

CREATE INDEX pgroonga_index ON logs
  USING pgroonga (record pgroonga.jsonb_ops_v2);

SET enable_seqscan = off;
SET enable_indexscan = on;
SET enable_bitmapscan = off;

SELECT id, record
  FROM logs
 WHERE record @> '{"body": {"values": ["Hello", true, 100]}}'::jsonb
 ORDER BY id;

DROP TABLE logs;
