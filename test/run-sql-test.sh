#!/bin/bash

source_dir=$(cd $(dirname $0) && pwd)
top_source_dir=${source_dir}/..

: ${BUILD_DIR:=$(pwd)}

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
      test_name=$(echo "${arg}" | sed -E -e 's,^sql/|\.sql,,g')
      expected_dir=${top_source_dir}/expected/$(dirname ${test_name})
      expected_path=${expected_dir}/$(basename ${test_name}).out
      if [ ! -f ${expected_path} ]; then
        mkdir -p ${expected_dir}
        touch ${expected_path}
      fi
      mkdir -p ${BUILD_DIR}/results/$(dirname ${test_name})
      test_names="${test_names[@]} ${test_name}"
      ;;
    *)
      if [ -d "${arg}" ]; then
        for test_path in $(find ${arg} -name "*.sql"); do
          test_name=$(echo "${test_path}" | sed -E -e 's,^sql/|\.sql,,g')
          test_names="${test_names[@]} ${test_name}"
        done
      else
        test_names="${test_names[@]} ${arg}"
      fi
      ;;
  esac
done

meson compile -C ${BUILD_DIR}
if [ "${NEED_SUDO:-no}" = "yes" ]; then
  sudo -H meson install -C ${BUILD_DIR} --no-rebuild > /dev/null
else
  meson install -C ${BUILD_DIR} --no-rebuild > /dev/null
fi
PG_REGRESS_DIFF_OPTS="-u"
if diff --help | grep -q color; then
  PG_REGRESS_DIFF_OPTS="${PG_REGRESS_DIFF_OPTS} --color=always"
fi
export PG_REGRESS_DIFF_OPTS
extra_regress_opts=()
if [ -n "${TEMP_INSTANCE:-}" ]; then
  extra_regress_opts+=("--temp-instance=${TEMP_INSTANCE}")
fi
if [ -n "${test_names}" ]; then
  pg_regress=$(pg_config --pkglibdir)/pgxs/src/test/regress/pg_regress
  ${pg_regress} \
    --bindir $(pg_config --bindir) \
    --dlpath ${BUILD_DIR} \
    --inputdir ${top_source_dir} \
    --outputdir ${BUILD_DIR} \
    --launcher ${source_dir}/short-pgappname \
    --load-extension pgroonga \
    "${extra_regress_opts[@]}" \
    ${test_names}
else
  meson test -C ${BUILD_DIR} -v
fi
success=$?
if [ $success != 0 ]; then
  cat ${BUILD_DIR}/regression.diffs
fi
exit $success
