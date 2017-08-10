CREATE TABLE ids (
  id integer
);

INSERT INTO ids VALUES (1);
INSERT INTO ids VALUES (2);
INSERT INTO ids VALUES (3);

CREATE INDEX grnindex ON ids
  USING pgroonga (id pgroonga.int4_ops);

SET enable_seqscan = off;
SET enable_indexscan = off;
SET enable_bitmapscan = on;

SELECT id
  FROM ids
 WHERE id <= 2;

DROP TABLE ids;
