#!/bin/bash

set -eux

function copy_logs() {
  log_dir=/host-rw/logs
  mkdir -p ${log_dir}
  cp -a ${data_dir}/log ${log_dir}/postgresql || :
  mkdir -p ${log_dir}/pgroonga/ || :
  cp -a ${data_dir}/pgroonga*.log* ${log_dir}/pgroonga/ || :
  chmod -R go+rx ${log_dir}
}

function run_test() {
  echo "::group::Run test"

  set +e
  ${pg_regress} \
    --launcher=/host/test/short-pgappname \
    --load-extension=pgroonga \
    --schedule=schedule
  pg_regress_status=$?
  set -e

  echo "::endgroup::"


  if [ ${pg_regress_status} -ne 0 ]; then
    echo "::group::Diff"
    cat regression.diffs
    echo "::endgroup::"
    copy_logs
    cp -a regression.diffs /host-rw/logs/
    exit ${pg_regress_status}
  fi


  echo "::group::Test primary-maintainer"

  set +e
  $(${pg_config} --bindir)/createuser postgres --superuser
  /host/packages/test-primary-maintainer.sh ${pg_config}
  test_status=$?
  $(${pg_config} --bindir)/dropuser postgres
  set -e

  echo "::endgroup::"


  if [ ${test_status} -ne 0 ]; then
    copy_logs
    exit ${test_status}
  fi
}

echo "::group::Prepare repositories"

os=$(cut -d: -f4 /etc/system-release-cpe)
case ${os} in
  almalinux)
    major_version=$(cut -d: -f5 /etc/system-release-cpe | grep -E -o "^[0-9]+")
    case ${major_version} in
      8)
        DNF="dnf --enablerepo=powertools"
        ${DNF} module -y disable postgresql
        ;;
      *)
        DNF="dnf --enablerepo=crb"
        ;;
    esac

    ${DNF} install -y \
           https://packages.apache.org/artifactory/arrow/${os}/${major_version}/apache-arrow-release-latest.rpm \
           https://download.postgresql.org/pub/repos/yum/reporpms/EL-${major_version}-x86_64/pgdg-redhat-repo-latest.noarch.rpm \
           https://packages.groonga.org/${os}/${major_version}/groonga-release-latest.noarch.rpm
    ;;
  fedora)
    major_version=$(cut -d: -f5 /etc/system-release-cpe | grep -o "^[0-9]")
    DNF="dnf"
    ;;
esac

echo "::endgroup::"


echo "::group::Install built packages"

packages_dir=/host/repositories/${os}/${major_version}/x86_64/Packages

pgroonga_package=$(basename $(ls ${packages_dir}/*-pgroonga-*.rpm | head -n1) | \
                     sed -e 's/-pgroonga-.*$/-pgroonga/g')
postgresql_version=$(echo ${pgroonga_package} | grep -E -o '[0-9.]+')

${DNF} install -y postgresql${postgresql_version}-contrib
${DNF} install -y ${packages_dir}/*.rpm

echo "::endgroup::"


echo "::group::Install packages for test"

case ${os} in
  almalinux)
    postgresql_package_prefix=$(rpm -qa | \
                                  grep pgroonga | \
                                  grep -E -o '^postgresql[0-9.]+' | \
                                  sed -e 's/\.//g')
    ${DNF} install -y ${postgresql_package_prefix}-devel
    pg_config=$(echo /usr/pgsql-*/bin/pg_config)
    groonga_token_filter_stem_package_name=groonga-token-filter-stem
    ;;
  *)
    ${DNF} install -y \
           mecab-ipadic \
           postgresql-devel \
           postgresql-server-devel
    pg_config=pg_config
    groonga_token_filter_stem_package_name=groonga-plugin-token-filters
    ;;
esac

${DNF} install -y \
       ${groonga_token_filter_stem_package_name} \
       bc \
       diffutils \
       groonga-tokenizer-mecab \
       ruby \
       sudo

echo "::endgroup::"


echo "::group::Prepare test"

data_dir=/tmp/data
sudo -u postgres -H \
     $(${pg_config} --bindir)/initdb \
     --encoding=UTF-8 \
     --locale=C \
     --pgdata=${data_dir} \
     --username=root
{
  echo "max_prepared_transactions = 1"
  echo "pgroonga.enable_wal = yes"
} | sudo -u postgres -H tee --append ${data_dir}/postgresql.conf
sudo -u postgres -H \
     $(${pg_config} --bindir)/pg_ctl start \
     --pgdata=${data_dir}

cp -a \
   /host/sql \
   /host/expected \
   /tmp/
cd /tmp
case "${os}" in
  fedora)
    # Require Groonga 10.1.0 or later.
    rm sql/function/highlight-html/one-keyword.sql
    rm sql/function/match-positions-byte/one-keyword.sql
    rm sql/function/match-positions-character/one-keyword.sql
    ;;
esac
ruby /host/test/prepare.rb > schedule
PG_REGRESS_DIFF_OPTS="-u"
if diff --help | grep -q color; then
  PG_REGRESS_DIFF_OPTS="${PG_REGRESS_DIFF_OPTS} --color=always"
fi
export PG_REGRESS_DIFF_OPTS
pg_regress=$(dirname $(${pg_config} --pgxs))/../test/regress/pg_regress

echo "::endgroup::"

run_test

echo "::group::Upgrade"

${DNF} remove -y ${pgroonga_package}

if ${DNF} info ${pgroonga_package} > /dev/null 2>&1; then
  is_first_release=no
else
  is_first_release=yes
fi

if [ "${is_first_release}" = "yes" ]; then
  echo "Skip because ${pgroonga_package} hasn't been released yet."
else
  ${DNF} install -y ${pgroonga_package}

  # We can use "rpm -q" for the latest released ${pgroonga_package} while
  # the latest released ${pgroonga_package} is installed.
  #
  # Example: postgresql16-pgdg-pgroonga-3.2.4-1.el8.x86_64
  pgroonga_latest_released_package=$(rpm -q ${pgroonga_package})
  # Example: 3.2.4
  pgroonga_latest_released_version=$(rpm -q --queryformat="%{VERSION}" ${pgroonga_package})

  createdb upgrade
  psql upgrade -c 'CREATE EXTENSION pgroonga'
  ${DNF} install -y ${packages_dir}/*.rpm
  psql upgrade -c 'ALTER EXTENSION pgroonga UPDATE'
fi

echo "::endgroup::"

echo "::group::Downgrade"

if [ "${is_first_release}" = "yes" ]; then
  echo "Skip because ${pgroonga_package} hasn't been released yet."
else
  createdb downgrade
  psql downgrade -c 'CREATE EXTENSION pgroonga'
  psql downgrade \
    -c "ALTER EXTENSION pgroonga UPDATE TO '${pgroonga_latest_released_version}'"
  ${DNF} install -y ${pgroonga_latest_released_package}
fi

echo "::endgroup::"

echo "::group::Postpare"

sudo -u postgres -H \
     $(${pg_config} --bindir)/pg_ctl stop \
     --pgdata=${data_dir}

echo "::endgroup::"
