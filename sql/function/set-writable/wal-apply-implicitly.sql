SET pgroonga.enable_wal = true;

CREATE TABLE memos (
  content text
);

CREATE INDEX pgrn_index ON memos USING PGroonga (content);

CREATE TABLE pgroonga_wal (
  applied_position bigint
);

SELECT pgroonga_command('object_exist',
                        ARRAY[
                          'name', 'IndexStatuses'
                        ])::jsonb->>1;

INSERT INTO pgroonga_wal (
  SELECT (pgroonga_command(
    'select',
     ARRAY[
      'table', 'IndexStatuses',
      'filter', '_key == ' ||
                'pgrn_index'::regclass::oid::text,
      'output_columns', 'wal_applied_position'
     ])::jsonb->1->0->2->>0)::bigint);

INSERT INTO memos VALUES ('Groonga is fast!');

SELECT pgroonga_command('load',
                        ARRAY[
                          'table', 'IndexStatuses',
                          'values',
                             '[{"_key": ' ||
                             'pgrn_index'::regclass::oid::text ||
                             ', "wal_applied_position": ' ||
                             (SELECT applied_position FROM pgroonga_wal) ||
                            '}]'
                        ])::jsonb->>1;
SELECT pgroonga_command('truncate',
                        ARRAY[
                          'table', pgroonga_table_name('pgrn_index')
                        ])::jsonb->>1;

SET enable_seqscan = false;

SELECT pgroonga_set_writable(false);

SELECT * FROM memos WHERE content &@~ 'Groonga';

SELECT pgroonga_set_writable(true);

SELECT * FROM memos WHERE content &@~ 'Groonga';

DROP TABLE memos;
