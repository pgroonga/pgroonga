CREATE TABLE ids (
  id integer,
  memo text
);

CREATE INDEX pgroonga_index ON ids USING pgroonga (id, memo);

INSERT INTO ids VALUES (2, 'a');
INSERT INTO ids VALUES (7, 'a');
INSERT INTO ids VALUES (6, 'a');
INSERT INTO ids VALUES (4, 'a');
INSERT INTO ids VALUES (5, 'a');
INSERT INTO ids VALUES (8, 'a');
INSERT INTO ids VALUES (1, 'a');
INSERT INTO ids VALUES (10, 'a');
INSERT INTO ids VALUES (3, 'a');
INSERT INTO ids VALUES (9, 'a');
INSERT INTO ids VALUES (12, 'a');
INSERT INTO ids VALUES (17, 'a');
INSERT INTO ids VALUES (16, 'a');
INSERT INTO ids VALUES (14, 'a');
INSERT INTO ids VALUES (15, 'a');
INSERT INTO ids VALUES (18, 'a');
INSERT INTO ids VALUES (11, 'a');
INSERT INTO ids VALUES (110, 'a');
INSERT INTO ids VALUES (13, 'a');
INSERT INTO ids VALUES (19, 'a');

SET enable_seqscan = off;
SET enable_indexscan = on;
SET enable_bitmapscan = off;

DELETE FROM ids WHERE id = 10;
DELETE FROM ids WHERE id = 5;
DELETE FROM ids WHERE id = 6;
SELECT * FROM ids WHERE 2 <= id and memo &@~ 'a';

DROP TABLE ids;
