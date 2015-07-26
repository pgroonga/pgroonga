-- To load PGroonga
SELECT pgroonga.command('status')::json->0->0;

SHOW pgroonga.windows_event_source_name;
SET pgroonga.windows_event_source_name = 'Groonga';
SHOW pgroonga.windows_event_source_name;
SET pgroonga.windows_event_source_name = default;
SHOW pgroonga.windows_event_source_name;
