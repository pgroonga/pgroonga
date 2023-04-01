CREATE TABLE data (
  id uuid,
  user_name text
);

CREATE USER alice NOLOGIN;
GRANT ALL ON TABLE data TO alice;

INSERT INTO data VALUES ('12345670-abcd-bcde-cdef-123456789012', 'nonexistent');
INSERT INTO data VALUES ('12345670-abcd-bcde-cdef-123456789012', 'alice');
INSERT INTO data VALUES ('12345671-abcd-bcde-cdef-123456789012', 'alice');
INSERT INTO data VALUES ('12345672-abcd-bcde-cdef-123456789012', 'alice');

ALTER TABLE data ENABLE ROW LEVEL SECURITY;
CREATE POLICY data_myself ON data USING (user_name = current_user);

CREATE INDEX pgrn_index ON data USING pgroonga (id);

SET enable_seqscan = off;
SET enable_indexscan = off;
SET enable_bitmapscan = on;

SET SESSION AUTHORIZATION alice;
\pset format unaligned
EXPLAIN (COSTS OFF)
SELECT id
  FROM data
 WHERE id = '12345670-ABCD-BCDE-CDEF-123456789012';
\g |sed -r -e "s/\(CURRENT_USER\)::text/CURRENT_USER/g"
\pset format aligned

SELECT id
  FROM data
 WHERE id = '12345670-ABCD-BCDE-CDEF-123456789012';
RESET SESSION AUTHORIZATION;

DROP TABLE data;

DROP USER alice;
