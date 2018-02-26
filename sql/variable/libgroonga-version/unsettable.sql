-- To load PGroonga
SELECT pgroonga_command('status')::json->0->0;

SET pgroonga.libgroonga_version = '8.0.2';
