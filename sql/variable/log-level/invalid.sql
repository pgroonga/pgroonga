-- To load PGroonga
SELECT pgroonga.command('status')::json->0->0;

SHOW pgroonga.log_level;
SET pgroonga.log_level = 'invalid';
SHOW pgroonga.log_level;
