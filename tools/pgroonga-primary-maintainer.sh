#!/usr/bin/env bash

set -xueo pipefail

WAL_BLOCK_SIZE=8192

numfmt_available=0
if type numfmt >/dev/null 2>&1; then
  numfmt_available=1
fi

reindex_threshold_size=0
psql_command=psql
psql_database_name=""
psql_options=""

function usage () {
  size_option_example="--thresholds 10M, -s 1G"
  if [ ${numfmt_available} -eq 0 ]; then
    size_option_example="--thresholds 10485760"
  fi
  cat <<USAGE
$0 --thresholds REINDEX_THRESHOLD_SIZE [--psql PSQL_COMMAND_PATH] [--dbname DATABASE_NAME] [--psql_options PSQL_COMMAND_OPTIONS]

Options:
-t, --thresholds:
  If the specified value is exceeded, \`REINDEX INDEX CONCURRENTLY\` is run.
  Specify by size.
  Example: ${size_option_example}
-c, --psql:
  Specify the path to \`psql\` command.
-d, --dbname:
  Specify the database name.
-o, --psql-options:
  \`psql\` command options.
  Example: --psql_options "-h example.com -p 5432"
-h, --help:
  Display help text and exit.
USAGE
}

function run_psql () {
  sql="${1}"
  "${psql_command}" \
    ${psql_options} \
    --tuples-only \
    --command "${sql}"
}

short_options="t:c:d:o:h"
long_options="thresholds:,psql:,dbname:,psql-options:,help"

# If you run `getopt` with no arguments and get an error,
# you are in an environment where the `--longoptions` option is available.
if getopt > /dev/null; then
  while getopts "${short_options}" opt; do
    case "${opt}" in
      t)
        if [ ${numfmt_available} -eq 1 ]; then
          reindex_threshold_size=$(numfmt --from iec "${OPTARG}")
        else
          reindex_threshold_size="${OPTARG}"
        fi
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
else
  options=$(
    getopt \
      --options "${short_options}" \
      --longoptions "${long_options}" \
      --name "${0}" \
      -- "$@"
  )
  eval set -- "$options"

  while [[ $# -gt 0 ]]; do
    case "${1}" in
      -t|--thresholds)
        if [ ${numfmt_available} -eq 1 ]; then
          reindex_threshold_size=$(numfmt --from iec "${2}")
        else
          reindex_threshold_size="${2}"
        fi
        shift 2
        ;;
      -c|--psql)
        psql_command="${2}"
        shift 2
        ;;
      -d|--dbname)
        psql_database_name="${2}"
        shift 2
        ;;
      -o|--psql_options)
        psql_options="${2}"
        shift 2
        ;;
      -h|--help)
        usage
        exit 0
        ;;
      --)
        shift
        break
        ;;
      *)
        usage
        exit 1
        ;;
    esac
  done
fi

if ! "${psql_command}" --help > /dev/null; then
  echo 'No psql command.'
  exit 1
fi

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
