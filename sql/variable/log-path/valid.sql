-- To load PGroonga
SELECT pgroonga_command('status')::json->0->0;

SHOW pgroonga.log_path;
SET pgroonga.log_path = 'none';
SHOW pgroonga.log_path;
SET pgroonga.log_path = default;
SHOW pgroonga.log_path;
