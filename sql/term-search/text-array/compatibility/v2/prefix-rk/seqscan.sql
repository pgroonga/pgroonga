CREATE TABLE readings (
  katakanas text[]
);

INSERT INTO readings VALUES (ARRAY['ポストグレスキューエル', 'ポスグレ']);
INSERT INTO readings VALUES (ARRAY['グルンガ', 'グルン']);
INSERT INTO readings VALUES (ARRAY['ピージールンガ', 'ピーグルン']);

SET enable_seqscan = on;
SET enable_indexscan = off;
SET enable_bitmapscan = off;

SELECT katakanas
  FROM readings
 WHERE katakanas &^~> 'p';

DROP TABLE readings;
