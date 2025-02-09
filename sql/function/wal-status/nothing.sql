SET pgroonga.enable_wal = yes;

SELECT * FROM pgroonga_wal_status();

SET pgroonga.enable_wal = default;
