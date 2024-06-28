#!/usr/bin/env bash

set -ue

run_times=()

function usage () {
  cat <<USAGE
Options:
--time:
  Specify run time,
  Example: --time 2:00 --time 3:30 ...
--help:
  Display help text and exit.
USAGE
}

long_options="time:,help"
options=$(
  getopt \
    --options "" \
    --longoptions "${long_options}" \
    --name "${0}" \
    -- "${@}"
)
eval set -- "$options"

while [[ $# -gt 0 ]]; do
  case "${1}" in
    --time)
      run_times+=("${2}")
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

if [ ${#run_times[@]} -eq 0 ]; then
  echo "Specify run time with \`--time\`."
  exit 1
fi

cat <<SERVICE
# How to install:
#   ${0} | sudo -H tee /lib/systemd/system/pgroonga-primary-maintainer.timer
#   sudo -H systemctl daemon-reload
#
# Usage:
#
#   Enable:  sudo -H systemctl enable --now pgroonga-primary-maintainer.timer
#   Disable: sudo -H systemctl disable --now pgroonga-primary-maintainer.timer
[Unit]
Description=PGroonga primary maintainer
[Timer]
$(
  for time in "${run_times[@]}"; do
    echo "OnCalendar=*-*-* ${time}:00"
  done
)
[Install]
WantedBy=timers.target
SERVICE
