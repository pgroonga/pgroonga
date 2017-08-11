-- To load PGroonga
SELECT pgroonga_command('status')::json->0->0;

SHOW pgroonga.query_log_path;
SET pgroonga.query_log_path = 'pgroonga.query.log';
SHOW pgroonga.query_log_path;
SET pgroonga.query_log_path = default;
SHOW pgroonga.query_log_path;
