SELECT
   json_typeof(
     pgroonga_command('status',
                      ARRAY[
                        'command_version', '1'
                      ])::json
  ) AS envelope_type;
