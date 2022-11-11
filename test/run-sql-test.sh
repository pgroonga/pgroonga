#!/bin/bash

run()
{
  "$@"
  if test $? -ne 0; then
    echo "Failed $@"
    exit 1
  fi
}

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

if [ "${SUPPRESS_LOG:-yes}" = "yes" ]; then
  OUTPUT="> /dev/null"
else
  OUTPUT=""
fi

: ${PG_CONFIG:=pg_config}

export PGRN_DEBUG=1
export HAVE_MSGPACK=1
eval "run make -j$(nproc) PG_CONFIG=${PG_CONFIG} ${OUTPUT}"
if [ "${NEED_SUDO:-no}" = "yes" ]; then
  eval "run sudo -H make PG_CONFIG=${PG_CONFIG} install ${OUTPUT}"
else
  eval "run make PG_CONFIG=${PG_CONFIG} install ${OUTPUT}"
fi
PG_REGRESS_DIFF_OPTS="-u"
if diff --help | grep -q color; then
  PG_REGRESS_DIFF_OPTS="${PG_REGRESS_DIFF_OPTS} --color=always"
fi
export PG_REGRESS_DIFF_OPTS
launcher="--launcher=$(pwd)/test/short-pgappname"
if [ -n "${test_names}" ]; then
  make installcheck \
       EXTRA_REGRESS_OPTS="${launcher}" \
       PG_CONFIG="${PG_CONFIG}" \
       REGRESS="${test_names}"
else
  make installcheck \
       EXTRA_REGRESS_OPTS="${launcher}" \
       PG_CONFIG="${PG_CONFIG}"
fi
success=$?
if [ $success != 0 ]; then
  cat regression.diffs
fi
exit $success
