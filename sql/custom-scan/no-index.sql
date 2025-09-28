CREATE TABLE ids (
  id integer
);

INSERT INTO ids VALUES (1);
INSERT INTO ids VALUES (2);
INSERT INTO ids VALUES (3);

SET pgroonga.enable_custom_scan = on;

EXPLAIN (COSTS OFF)
SELECT id
  FROM ids
 WHERE id = 2;

SELECT id
  FROM ids
 WHERE id = 2;

DROP TABLE ids;
