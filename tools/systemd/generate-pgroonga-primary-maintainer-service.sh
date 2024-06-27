#!/usr/bin/env bash

set -xue

pgroonga_primary_maintainer_command=$(which pgroonga-primary-maintainer.sh || :)
threshold="1G"
psql_command=""
environments=()
on_failure_service=""

function usage () {
  cat <<USAGE
Options:
-s, --pgroonga-primary-maintainer-command:
  Specify the path to \`pgroonga-primary-maintainer.sh\`
  (default: ${pgroonga_primary_maintainer_command})
-t, --threshold:
  If the specified value is exceeded, \`REINDEX INDEX CONCURRENTLY\` is run.
  (default: ${threshold})
-e, --environment
  Connection information such as \`dbname\` should be set in environment variables.
  See also: https://www.postgresql.org/docs/current/libpq-envars.html"
  Example: --environment KEY1=VALUE1 --environment KEY2=VALUE2 ...
-c, --psql:
  Specify the path to \`psql\` command.
-f, --on-failure-service:
  Run SERVICE on failure
-h, --help:
  Display help text and exit.
USAGE
}

short_options="s:t:e:c:f:h"
long_options="pgroonga-primary-maintainer-command:,threshold:,environment:,psql:,on-failure-service:,help"
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
    -s|--pgroonga-primary-maintainer-command)
      pgroonga_primary_maintainer_command="${2}"
      shift 2
      ;;
    -t|--threshold)
      threshold="${2}"
      shift 2
      ;;
    -e|--environment)
      environments+=("${2}")
      shift 2
      ;;
    -c|--psql)
      psql_command="${2}"
      shift 2
      ;;
    -f|--on-failure-service)
      on_failure_service="${2}"
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

if ! "${pgroonga_primary_maintainer_command}" --help > /dev/null; then
  echo "No pgroonga-primary-maintainer.sh command."
  exit 1
fi

pgroonga_primary_maintainer_options="--threshold ${threshold}"
if [ -n "${psql_command}" ]; then
  pgroonga_primary_maintainer_options+=" --psql ${psql_command}"
fi

on_failure=""
if [ -n "${on_failure_service}" ]; then
  on_failure="OnFailure=${on_failure_service}"
fi

user_name=$(whoami)
group_name=$(groups | awk '{print $1}')
cat <<SERVICE
# How to install:
#   ${0} | sudo -H tee /lib/systemd/system/pgroonga-primary-maintainer.service
[Unit]
Description=PGroonga primary maintainer
${on_failure}
[Service]
Type=oneshot
User=${user_name}
Group=${group_name}
Environment=${environments[@]}
ExecStart=${pgroonga_primary_maintainer_command} ${pgroonga_primary_maintainer_options}
[Install]
WantedBy=multi-user.target
SERVICE
