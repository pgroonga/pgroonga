CREATE TABLE logs (
  created_at timestamp
);

INSERT INTO logs VALUES ('2015-02-14 10:44:10');
INSERT INTO logs VALUES ('2015-02-14 10:44:09');
INSERT INTO logs VALUES ('2015-02-14 10:44:02');
INSERT INTO logs VALUES ('2015-02-14 10:44:04');
INSERT INTO logs VALUES ('2015-02-14 10:44:01');
INSERT INTO logs VALUES ('2015-02-14 10:44:05');
INSERT INTO logs VALUES ('2015-02-14 10:44:07');
INSERT INTO logs VALUES ('2015-02-14 10:44:06');
INSERT INTO logs VALUES ('2015-02-14 10:44:03');
INSERT INTO logs VALUES ('2015-02-14 10:44:08');

CREATE INDEX pgroonga_index ON logs USING pgroonga (created_at);

SET enable_seqscan = on;
SET enable_indexscan = off;
SET enable_bitmapscan = off;

SELECT created_at
  FROM logs
 WHERE created_at BETWEEN '2015-02-14 10:44:03' AND '2015-02-14 10:44:09'
 ORDER BY created_at ASC;

DROP TABLE logs;
