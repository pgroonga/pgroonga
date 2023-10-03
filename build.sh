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
cp -a "${source_directory}" "${build_directory}"
cd "${build_directory}"
make clean
export HAVE_XXHASH=1
export NEED_SUDO=yes
export TEMP_INSTANCE="${PWD}/db"
test/run-sql-test.sh "$@"
