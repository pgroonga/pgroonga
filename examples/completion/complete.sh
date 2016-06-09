#!/bin/sh

if [ $# != 1 ]; then
  echo "Usage: $0 INPUT | psql DB"
  echo " e.g.: $0 nih | psql DB"
  exit 1
fi

input=$1

cat <<SQL
SET enable_seqscan = no;
SELECT term, readings, english
  FROM dictionary
 WHERE term &^ '$1' OR
       readings &^~> '$1';
SQL
