#!/bin/bash

if [ $# -lt 2 ]; then
  echo "Usage: $0 SOURCE_DIRECTORY BUILD_DIRECTORY [OPTIONS...]"
  echo " e.g.: $0 . /tmp/build"
  echo " e.g.: $0 . /tmp/build sql/full-text-search/text/single/query-v2/indexscan.sql"
  exit 1
fi

set -eux

source_directory="$1"
shift
build_directory="$1"
shift

rm -rf "${build_directory}"
meson setup "${build_directory}" "${source_directory}"
export BUILD_DIR="${build_directory}"
export NEED_SUDO="yes"
export TEMP_INSTANCE="${build_directory}/db"
"${source_directory}/test/run-sql-test.sh" "$@"
