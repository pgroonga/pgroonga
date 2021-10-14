CREATE TABLE readings (
  id integer,
  user_name text,
  katakanas text[]
);

CREATE USER alice NOLOGIN;
GRANT ALL ON TABLE readings TO alice;

INSERT INTO readings VALUES
  (1, 'nonexistent', ARRAY['ポストグレスキューエル', 'ポスグレ']);
INSERT INTO readings VALUES (2, 'alice', ARRAY['グルンガ', 'グルン']);
INSERT INTO readings VALUES (3, 'alice', ARRAY['ピージールンガ', 'ピーグルン']);

ALTER TABLE readings ENABLE ROW LEVEL SECURITY;
CREATE POLICY readings_myself ON readings USING (user_name = current_user);

CREATE INDEX pgrn_index ON readings
 USING pgroonga (katakanas pgroonga_text_array_term_search_ops_v2);

SET enable_seqscan = off;
SET enable_indexscan = on;
SET enable_bitmapscan = off;

SET SESSION AUTHORIZATION alice;
\pset format unaligned
EXPLAIN (COSTS OFF)
SELECT katakanas
  FROM readings
 WHERE katakanas &^~ 'p'
 ORDER BY id
\g |sed -r -e "s/\(CURRENT_USER\)::text/CURRENT_USER/g"
\pset format aligned

SELECT katakanas
  FROM readings
 WHERE katakanas &^~ 'p'
 ORDER BY id;
RESET SESSION AUTHORIZATION;

DROP TABLE readings;

DROP USER alice;
