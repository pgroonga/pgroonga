SELECT jsonb_pretty(token::jsonb)
  FROM (
    SELECT
      unnest(pgroonga_tokenize('これはペンです。',
                               'tokenizer', 'TokenMecab("include_class", true)'))
        AS token
  ) AS tokens;
