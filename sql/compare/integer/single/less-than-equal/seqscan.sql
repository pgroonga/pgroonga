CREATE TABLE ids (
  id integer
);

INSERT INTO ids VALUES (1);
INSERT INTO ids VALUES (2);
INSERT INTO ids VALUES (3);

CREATE INDEX grnindex ON ids USING pgroonga (id);

SET enable_seqscan = on;
SET enable_indexscan = off;
SET enable_bitmapscan = off;

SELECT id
  FROM ids
 WHERE id <= 2;

DROP TABLE ids;
