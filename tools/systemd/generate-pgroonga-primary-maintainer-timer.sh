#!/usr/bin/env bash

set -xue

run_times=()

function usage () {
  cat <<USAGE
Options:
-t, --time:
  Specify run time,
  Example: --time 2:00 --time 3:30 ...
-h, --help:
  Display help text and exit.
USAGE
}

short_options="t:h"
long_options="time:,help"
# If you run `getopt` with no arguments and get an error,
# you are in an environment where the `--longoptions` option is available.
if getopt > /dev/null; then
  options=$(getopt "${short_options}" "$@")
else
  options=$(
    getopt \
      --options "${short_options}" \
      --longoptions "${long_options}" \
      --name "${0}" \
      -- "$@"
  )
fi
eval set -- "$options"

while [[ $# -gt 0 ]]; do
  case "${1}" in
    -t|--time)
      run_times+=("${2}")
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
  for time in ${run_times[@]}; do
    echo "OnCalendar=*-*-* ${time}:00"
  done
)
[Install]
WantedBy=timers.target
SERVICE
