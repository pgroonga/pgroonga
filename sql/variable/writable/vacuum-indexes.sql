CREATE TABLE memos (
  content text
);

CREATE INDEX pgrn_index1 ON memos USING PGroonga (content);
CREATE INDEX pgrn_index2 ON memos USING PGroonga (content);

INSERT INTO memos VALUES ('Groonga is fast!');
DELETE FROM memos;

CREATE TABLE pgrn_index_oids (
  oid oid
);
INSERT INTO pgrn_index_oids (SELECT 'pgrn_index2'::regclass::oid);

DROP INDEX pgrn_index2;

SET pgroonga.writable = false;

VACUUM memos;
SELECT pgroonga_command('object_exist',
                        ARRAY[
                          'name', 'Sources' || (SELECT oid FROM pgrn_index_oids)
                        ])::jsonb->>1;

SET pgroonga.writable = true;

INSERT INTO memos VALUES ('PGroonga is fast!');
DELETE FROM memos;

VACUUM memos;
SELECT pgroonga_command('object_exist',
                        ARRAY[
                          'name', 'Sources' || (SELECT oid FROM pgrn_index_oids)
                        ])::jsonb->>1;

DROP TABLE memos;
DROP TABLE pgrn_index_oids;
