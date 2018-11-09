SELECT jsonb_pretty(token::jsonb)
  FROM (
    SELECT unnest(pgroonga_tokenize('This is a pen.',
                                    'tokenizer', 'TokenNgram'))
           AS token
  ) AS tokens;
