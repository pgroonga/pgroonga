CREATE TABLE entries (
  id int PRIMARY KEY,
  title varchar(255),
  description varchar(255),
  memo varchar(255)
);

CREATE INDEX entries_fts_index
 ON entries
 USING pgroonga (
   (ARRAY[
      title,
      description,
      memo
    ]::text[])
 )
 WITH (normalizers='
         NormalizerTable(
           "normalized", "${table:normalizations_index}.normalized",
           "target", "target",
           "report_source_offset", true),
         NormalizerNFKC130
       ',
       tokenizer='
         TokenNgram("unify_alphabet", false,
                    "unify_digit", false,
                    "unify_symbol", false,
                    "report_source_location", true)
       ');
