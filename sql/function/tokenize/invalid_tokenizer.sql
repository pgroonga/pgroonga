-- 1st call: should error
SELECT pgroonga_tokenize('This is a pen.',
                         'tokenizer', 'invalid');
-- 2nd call with same invalid tokenizer: should also error
SELECT pgroonga_tokenize('This is a pen.',
                         'tokenizer', 'invalid');
