CREATE TABLE logs (
  record jsonb
);

CREATE INDEX pgroonga_index ON logs
  USING pgroonga (record pgroonga_jsonb_ops_v2);

INSERT INTO logs VALUES (NULL);

SELECT * FROM logs;

DROP TABLE logs;
