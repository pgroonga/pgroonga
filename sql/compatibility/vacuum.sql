-- To load PGroonga
SELECT pgroonga_command('status')::json->0->0;

-- Just for reducing disk usage. This tests nothing.
VACUUM;
