#!/usr/bin/env bash

set -xueo pipefail

WAL_BLOCK_SIZE=8192

reindex_threshold_size=0
psql_command=psql
psql_database_name=""
psql_options=""

function usage () {
  cat <<USAGE
$0 -s REINDEX_THRESHOLD_SIZE [-c PSQL_COMMAND_PATH] [-d DATABASE_NAME] [-o PSQL_COMMAND_OPTIONS]

Options:
-s:
  If the specified value is exceeded, \`REINDEX INDEX CONCURRENTLY\` is run.
  Specify by size.
  Example: -s 10M, -s 1G
-c:
  Specify the path to \`psql\` command.
-d:
  Specify the database name.
-o:
  \`psql\` command options.
  Example: -o "-h example.com -p 5432"
USAGE
}

function run_psql () {
  sql="${1}"
  ${psql_command} \
    ${psql_options} \
    --tuples-only \
    --command "${sql}"
}

while getopts 's:c:d:o:h' flag; do
  case "${flag}" in
    s)
      reindex_threshold_size=$(numfmt --from iec ${OPTARG})
      ;;
    c)
      psql_command="${OPTARG}"
      ;;
    d)
      psql_database_name="${OPTARG}"
      ;;
    o)
      psql_options="${OPTARG}"
      ;;
    h)
      usage
      exit 0
      ;;
    *)
      uasge
      exit 1
      ;;
  esac
done

test -x "${psql_command}" || (echo 'No psql command.' && exit 1)

if [ -n "${psql_database_name}" ]; then
  psql_options+=" --dbname ${psql_database_name}"
fi

reindex_threshold_block=$((reindex_threshold_size/WAL_BLOCK_SIZE))
if [ ${reindex_threshold_block} -lt 1 ]; then
  echo "Thresholds are too small. (${reindex_threshold_size})"
  exit 1
fi

wal_size_check_sql=$(cat <<SQL
SELECT
  name
FROM
  pgroonga_wal_status()
WHERE
  last_block >= ${reindex_threshold_block};
SQL
)

for index_name in $(run_psql "${wal_size_check_sql}"); do
  reindex_sql="REINDEX INDEX CONCURRENTLY ${index_name}"
  echo "Run '${reindex_sql}'"
  date
  run_psql "${reindex_sql}"
  date
done
