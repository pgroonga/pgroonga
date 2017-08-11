-- To load PGroonga
SELECT pgroonga_command('status')::json->0->0;

SHOW pgroonga.log_type;
SET pgroonga.log_type = 'postgresql';
SHOW pgroonga.log_type;
SET pgroonga.log_type = default;
SHOW pgroonga.log_type;
