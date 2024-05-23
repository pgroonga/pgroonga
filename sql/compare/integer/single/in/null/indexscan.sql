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

CREATE INDEX pgroonga_index ON ids USING pgroonga (id);

SET enable_seqscan = off;
SET enable_indexscan = on;
SET enable_bitmapscan = off;

SELECT id
  FROM ids
 WHERE id IN (6, NULL, 7)
 ORDER BY id ASC;

DROP TABLE ids;
