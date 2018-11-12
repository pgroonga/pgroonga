SELECT jsonb_pretty(token::jsonb)
  FROM (
    SELECT
      unnest(pgroonga_tokenize('これはペンです。',
                               'tokenizer', 'TokenNgram',
                               'token_filters', 'TokenFilterNFKC100("unify_kana", true)'))
      AS token
  ) AS tokens;
