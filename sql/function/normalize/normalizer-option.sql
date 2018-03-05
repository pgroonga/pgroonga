SELECT pgroonga_command('plugin_register normalizers/mysql')::json->1;
SELECT pgroonga_normalize('aBcDe 123', 'NormalizerMySQLGeneralCI');
