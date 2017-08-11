-- To load PGroonga
SELECT pgroonga_command('status')::json->0->0;

SHOW pgroonga.match_escalation_threshold;
SET pgroonga.match_escalation_threshold = -1;
SHOW pgroonga.match_escalation_threshold;
SET pgroonga.match_escalation_threshold = default;
SHOW pgroonga.match_escalation_threshold;
