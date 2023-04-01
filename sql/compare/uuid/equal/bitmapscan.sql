CREATE TABLE data (
  id uuid
);

INSERT INTO data VALUES ('12345678-abcd-bcde-cdef-123456789012');
INSERT INTO data VALUES ('12345670-abcd-bcde-cdef-123456789012');
INSERT INTO data VALUES ('12345671-abcd-bcde-cdef-123456789012');
INSERT INTO data VALUES ('12345672-abcd-bcde-cdef-123456789012');

CREATE INDEX pgrn_index ON data USING pgroonga (id);

SET enable_seqscan = off;
SET enable_indexscan = off;
SET enable_bitmapscan = on;

EXPLAIN (COSTS OFF)
SELECT id
  FROM data
 WHERE id = '12345670-ABCD-BCDE-CDEF-123456789012';

SELECT id
  FROM data
 WHERE id = '12345670-ABCD-BCDE-CDEF-123456789012';

DROP TABLE data;
