CREATE TABLE ids (
  id integer
);

INSERT INTO ids VALUES (2);
INSERT INTO ids VALUES (7);
INSERT INTO ids VALUES (6);
INSERT INTO ids VALUES (4);
INSERT INTO ids VALUES (5);
INSERT INTO ids VALUES (8);
INSERT INTO ids VALUES (1);
INSERT INTO ids VALUES (10);
INSERT INTO ids VALUES (3);
INSERT INTO ids VALUES (9);

CREATE INDEX grnindex ON ids
  USING pgroonga (id pgroonga.int4_ops);

SET enable_seqscan = off;
SET enable_indexscan = on;
SET enable_bitmapscan = off;

SELECT id
  FROM ids
 WHERE id <= 5 AND id <= 3
 ORDER BY id ASC;

DROP TABLE ids;
