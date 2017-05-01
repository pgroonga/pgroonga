#!/bin/bash

run()
{
  "$@"
  if test $? -ne 0; then
    echo "Failed $@"
    exit 1
  fi
}

set -x
test_names=""
while [ $# -gt 0 ]; do
  arg="$1"
  shift
  case "${arg}" in
    sql/*.sql)
      test_name=$(echo "${arg}" | sed -e 's,^sql/\|\.sql,,g')
      test_names="${test_names[@]} ${test_name}"
      ;;
    *)
      if [ -d "${arg}" ]; then
        for test_path in $(find ${arg} -name "*.sql"); do
          test_name=$(echo "${test_path}" | sed -e 's,^sql/\|\.sql,,g')
	  test_names="${test_names[@]} ${test_name}"
        done
      else
	test_names="${test_names[@]} ${arg}"
      fi
      ;;
  esac
done

DEBUG=1 HAVE_MSGPACK=1 run make -j$(nproc) > /dev/null
run make install > /dev/null
if [ -n "${test_names}" ]; then
  make installcheck REGRESS="${test_names}"
else
  make installcheck
fi
if [ $? != 0 ]; then
  cat regression.diffs
fi
