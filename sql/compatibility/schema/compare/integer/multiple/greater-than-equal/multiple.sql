CREATE TABLE numbers (
  number1 integer,
  number2 integer
);

INSERT INTO numbers VALUES (2,  20);
INSERT INTO numbers VALUES (7,  70);
INSERT INTO numbers VALUES (6,  60);
INSERT INTO numbers VALUES (4,  40);
INSERT INTO numbers VALUES (5,  50);
INSERT INTO numbers VALUES (8,  80);
INSERT INTO numbers VALUES (1,  10);
INSERT INTO numbers VALUES (10, 100);
INSERT INTO numbers VALUES (3,  30);
INSERT INTO numbers VALUES (9,  90);

CREATE INDEX grnindex ON numbers
  USING pgroonga (number1 pgroonga.int4_ops,
                  number2 pgroonga.int4_ops);

SET enable_seqscan = off;
SET enable_indexscan = on;
SET enable_bitmapscan = off;

SELECT number1, number2
  FROM numbers
 WHERE number1 >= 3 AND number2 >= 50
 ORDER BY number1 ASC;

DROP TABLE numbers;
