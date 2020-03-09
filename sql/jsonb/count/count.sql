CREATE TABLE logs (
  record jsonb
);

CREATE INDEX pgroonga_index ON logs USING pgroonga (record);

SET enable_seqscan = false;
SELECT COUNT(*) FROM logs;
DROP TABLE logs;
