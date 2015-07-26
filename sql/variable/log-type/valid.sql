-- To load PGroonga
SELECT pgroonga.command('status')::json->0->0;

SHOW pgroonga.log_type;
SET pgroonga.log_type = 'windows_event_log';
SHOW pgroonga.log_type;
SET pgroonga.log_type = default;
SHOW pgroonga.log_type;
