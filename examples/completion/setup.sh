#!/bin/sh

set -u
set -e

base_dir="$(dirname $0)"

edict_gz="${base_dir}/edict.gz"

if [ ! -f "${edict_gz}" ]; then
  wget -O "${edict_gz}" http://ftp.monash.edu.au/pub/nihongo/edict.gz
fi

cat <<SQL
CREATE EXTENSION IF NOT EXISTS pgroonga;

DROP TABLE IF EXISTS dictionary;
CREATE TABLE dictionary (
  term text,
  readings text[],
  english text
);

\\timing
SQL

zcat "${edict_gz}" | ./create-insert.rb

cat <<SQL

CREATE INDEX pgroonga_index
  ON dictionary
  USING pgroonga (term pgroonga.text_term_search_ops_v2,
                  readings pgroonga.text_array_term_search_ops_v2,
                  english pgroonga.text_full_text_search_ops_v2);
SQL
