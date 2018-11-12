SELECT jsonb_pretty(token::jsonb)
  FROM (
    SELECT
      unnest(pgroonga_tokenize('This is a pen.',
                               'tokenizer',
                                 'TokenNgram("report_source_location", true)',
                               'normalizer', 'NormalizerNFKC100'))
        AS token
  ) AS tokens;
