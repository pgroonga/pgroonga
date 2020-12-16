SET pgroonga.enable_wal = yes;

CREATE TABLE memos (
  content text
);

INSERT INTO memos
SELECT string_agg(str, '') 
  FROM 
  (SELECT chr(12449 + (random() * 1000)::int % 85 ) as str , i 
    FROM  generate_series(1,20) length, generate_series(1,100000) num(i)
   )t
   GROUP BY i;

CREATE INDEX pgrn_memos_index ON memos USING PGroonga (content);
SELECT pgroonga_wal_truncate();

DROP TABLE memos;
