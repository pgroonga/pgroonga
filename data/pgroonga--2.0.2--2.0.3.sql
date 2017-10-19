-- Set RESTRICT and JOIN
UPDATE pg_catalog.pg_operator
   SET oprrest = 'contsel',
       oprjoin = 'contjoinsel'
 WHERE oprcode::text LIKE 'pgroonga_%' OR
       oprcode::text LIKE 'public.pgroonga_%' OR
       oprcode::text LIKE 'pgroonga.%';
