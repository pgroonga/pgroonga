#!/bin/bash

set -eux

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
    mkdir -p /host-rw/logs
    cp -a regression.diffs /host-rw/logs/
    cp -a ${data_dir}/log /host-rw/logs/postgresql || :
    mkdir -p /host-rw/logs/pgroonga/ || :
    cp -a ${data_dir}/pgroonga.log* /host-rw/logs/pgroonga/ || :
    chmod -R go+rx /host-rw/logs/
    exit ${pg_regress_status}
  fi


  echo "::group::Test primary-maintainer"

  $(${pg_config} --bindir)/createuser postgres --superuser
  /host/packages/test-primary-maintainer.sh ${pg_config}
  $(${pg_config} --bindir)/dropuser postgres

  echo "::endgroup::"
}

echo "::group::Prepare repositories"

os=$(cut -d: -f4 /etc/system-release-cpe)
case ${os} in
  almalinux)
    major_version=$(cut -d: -f5 /etc/system-release-cpe | grep -o "^[0-9]")
    case ${major_version} in
      8)
        DNF="dnf --enablerepo=powertools"
        ${DNF} module -y disable postgresql
        ;;
      *)
        DNF="dnf --enablerepo=crb"
        ${DNF} install -y \
               https://apache.jfrog.io/artifactory/arrow/${os}/${major_version}/apache-arrow-release-latest.rpm
        ;;
    esac

    ${DNF} install -y \
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
  almalinux)
    if [ ${postgresql_version} -lt 12 ]; then
      rm sql/function/highlight-html/declarative-partitioning.sql
      rm sql/function/wal-set-applied-position/declarative-partitioning.sql
      rm sql/vacuum/two-phase-commit.sql
    fi
    if [ ${postgresql_version} -lt 13 ]; then
      rm sql/full-text-search/text/single/declarative-partitioning.sql
    fi
    ;;
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

  # "rpm -q" get installed packages information.
  # Therefore, we get PGroonga packages name and version after "${DNF} install -y ${pgroonga_package}".
  #
  # pgroonga_latest_released_version_full=postgresql16-pgdg-pgroonga-3.2.4-1.el8
  pgroonga_latest_released_package=$(rpm -q ${pgroonga_package})
  # The value of "pgroonga_latest_released_version" is shown below in this example.
  #
  # pgroonga_latest_released_version=3.2.4
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
  ${DNF} install -y ${pgroonga_latest_released_version_full}
fi

echo "::endgroup::"

echo "::group::Postpare"

sudo -u postgres -H \
     $(${pg_config} --bindir)/pg_ctl stop \
     --pgdata=${data_dir}

echo "::endgroup::"
