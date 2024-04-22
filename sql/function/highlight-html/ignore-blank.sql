CREATE TABLE names(
  name text
);

INSERT INTO names values ('山田　太郎');

CREATE INDEX names_index
  ON names
  USING pgroonga(name)
  WITH (
    normalizers='NormalizerNFKC150("report_source_offset",true)',
    tokenizer='TokenNgram("loose_blank", true,"report_source_location",true)'
  );

SELECT
  pgroonga_highlight_html(
    name,
    ARRAY['山田太郎'],
    'names_index'
  )
FROM
  names;

DROP TABLE names;
