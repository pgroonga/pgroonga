CREATE TABLE readings (
  katakana text
);

INSERT INTO readings VALUES ('ポストグレスキューエル');
INSERT INTO readings VALUES ('グルンガ');
INSERT INTO readings VALUES ('ピージールンガ');
INSERT INTO readings VALUES ('ピージーロジカル');

SET enable_seqscan = on;
SET enable_indexscan = off;
SET enable_bitmapscan = off;

SELECT katakana
  FROM readings
 WHERE katakana &^~| ARRAY['po', 'gu'];

DROP TABLE readings;
