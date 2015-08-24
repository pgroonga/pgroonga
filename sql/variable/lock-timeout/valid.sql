-- To load PGroonga
SELECT pgroonga.command('status')::json->0->0;

SHOW pgroonga.lock_timeout;
SET pgroonga.lock_timeout = 1000;
SHOW pgroonga.lock_timeout;
SET pgroonga.lock_timeout = default;
SHOW pgroonga.lock_timeout;
