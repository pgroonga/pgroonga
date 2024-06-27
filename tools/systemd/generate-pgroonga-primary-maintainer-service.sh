#!/usr/bin/env bash

set -ue

pgroonga_primary_maintainer_command=$(
  (which pgroonga-primary-maintainer.sh || :) | \
    sed --regexp-extended -e 's#/{2,}#/#g'
)
threshold="1G"
psql_command=""
environments=()
on_failure_service=""

function usage () {
  cat <<USAGE
Options:
--pgroonga-primary-maintainer-command:
  Specify the path to \`pgroonga-primary-maintainer.sh\`
  (default: ${pgroonga_primary_maintainer_command})
--threshold:
  If the specified value is exceeded, \`REINDEX INDEX CONCURRENTLY\` is run.
  (default: ${threshold})
--environment
  Connection information such as \`dbname\` should be set in environment variables.
  See also: https://www.postgresql.org/docs/current/libpq-envars.html"
  Example: --environment KEY1=VALUE1 --environment KEY2=VALUE2 ...
--psql:
  Specify the path to \`psql\` command.
--on-failure-service:
  Run SERVICE on failure
--help:
  Display help text and exit.
USAGE
}

long_options="pgroonga-primary-maintainer-command:,threshold:,environment:,psql:,on-failure-service:,help"
options=$(
  getopt \
    --options "" \
    --longoptions "${long_options}" \
    --name "${0}" \
    -- "$@"
)
eval set -- "$options"

while [[ $# -gt 0 ]]; do
  case "${1}" in
    --pgroonga-primary-maintainer-command)
      pgroonga_primary_maintainer_command="${2}"
      shift 2
      ;;
    --threshold)
      threshold="${2}"
      shift 2
      ;;
    --environment)
      environments+=("${2}")
      shift 2
      ;;
    --psql)
      psql_command="${2}"
      shift 2
      ;;
    --on-failure-service)
      on_failure_service="${2}"
      shift 2
      ;;
    --help)
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

pgroonga_primary_maintainer_command=$(realpath "$pgroonga_primary_maintainer_command" 2>/dev/null || :)
if ! "${pgroonga_primary_maintainer_command}" --help > /dev/null 2>&1; then
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

user_name=$(id --user --name)
group_name=$(id --group --name)
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
