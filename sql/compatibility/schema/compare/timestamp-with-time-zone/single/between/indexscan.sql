CREATE TABLE logs (
  created_at timestamp with time zone
);

INSERT INTO logs VALUES ('2015-02-14 10:44:10+09:00');
INSERT INTO logs VALUES ('2015-02-14 10:44:09+09:00');
INSERT INTO logs VALUES ('2015-02-14 10:44:02+09:00');
INSERT INTO logs VALUES ('2015-02-14 10:44:04+09:00');
INSERT INTO logs VALUES ('2015-02-14 10:44:01+09:00');
INSERT INTO logs VALUES ('2015-02-14 10:44:05+09:00');
INSERT INTO logs VALUES ('2015-02-14 10:44:07+09:00');
INSERT INTO logs VALUES ('2015-02-14 10:44:06+09:00');
INSERT INTO logs VALUES ('2015-02-14 10:44:03+09:00');
INSERT INTO logs VALUES ('2015-02-14 10:44:08+09:00');

CREATE INDEX pgroonga_index ON logs
  USING pgroonga (created_at pgroonga.timestamptz_ops);

SET enable_seqscan = off;
SET enable_indexscan = on;
SET enable_bitmapscan = off;

SELECT created_at
  FROM logs
 WHERE created_at BETWEEN '2015-02-14 10:44:03+09:00' AND '2015-02-14 10:44:09+09:00'
 ORDER BY created_at ASC;

DROP TABLE logs;
