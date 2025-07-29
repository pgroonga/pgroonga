#!/bin/bash

set -eux

function copy_logs() {
  log_dir=/host-rw/logs
  mkdir -p ${log_dir}
  cp -a /var/log/postgresql ${log_dir} || :
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
  $(pg_config --bindir)/createuser postgres --superuser
  /host/packages/test-primary-maintainer.sh
  test_status=$?
  $(pg_config --bindir)/dropuser postgres
  set -e

  echo "::endgroup::"


  if [ ${test_status} -ne 0 ]; then
    copy_logs
    exit ${test_status}
  fi
}

echo "::group::Prepare APT repositories"

echo "debconf debconf/frontend select Noninteractive" | \
  debconf-set-selections

apt update
apt install -V -y \
    lsb-release \
    wget

os=$(lsb_release --id --short | tr "A-Z" "a-z")
code_name=$(lsb_release --codename --short)
architecture=$(dpkg --print-architecture)

repositories_dir=/host/repositories

case "${os}-${code-name}" in
  debian-*)
    wget https://apache.jfrog.io/artifactory/arrow/${os}/apache-arrow-apt-source-latest-${code_name}.deb
    apt install -V -y ./apache-arrow-apt-source-latest-${code_name}.deb
    ;;
esac

wget \
  https://packages.groonga.org/${os}/groonga-apt-source-latest-${code_name}.deb
apt install -V -y ./groonga-apt-source-latest-${code_name}.deb

if [ "${os}" = "ubuntu" ]; then
  apt install -y software-properties-common
  add-apt-repository -y universe
  add-apt-repository -y ppa:groonga/ppa
fi

if find ${repositories_dir} | grep -q "pgdg"; then
  wget -O /usr/share/keyrings/pgdg.asc \
    https://www.postgresql.org/media/keys/ACCC4CF8.asc
  (echo "Types: deb"; \
   echo "URIs: http://apt.postgresql.org/pub/repos/apt"; \
   echo "Suites: ${code_name}-pgdg"; \
   echo "Components: main"; \
   echo "Signed-By: /usr/share/keyrings/pgdg.asc") | \
    tee /etc/apt/sources.list.d/pgdg.sources
fi

echo "::endgroup::"


echo "::group::Install built packages"

apt update

apt install -V -y \
  ${repositories_dir}/${os}/pool/${code_name}/*/*/*/*_${architecture}.deb

echo "::endgroup::"


echo "::group::Install packages for test"

pgroonga_package=$(dpkg -l | \
                     grep pgroonga | \
                     head -n1 | \
                     awk '{print $2}')
postgresql_version=$(echo ${pgroonga_package} | grep -E -o '[0-9.]+')
postgresql_package_prefix=$(echo ${pgroonga_package} | \
                              grep -E -o 'postgresql-[0-9.]+(-pgdg)?')
if ! echo "${postgresql_package_prefix}" | grep -q pgdg; then
  apt install -V -y \
      $(echo ${postgresql_package_prefix} | \
          sed -e 's/^postgresql-/postgresql-server-dev-/')
fi

apt install -V -y \
    groonga-token-filter-stem \
    groonga-tokenizer-mecab \
    ruby \
    sudo

echo "::endgroup::"


echo "::group::Prepare test"

systemctl stop postgresql
data_dir=/tmp/data
sudo -u postgres -H \
     $(pg_config --bindir)/initdb \
     --encoding=UTF-8 \
     --locale=C \
     --pgdata=${data_dir} \
     --username=root
{
  echo "max_prepared_transactions = 1"
  echo "pgroonga.enable_wal = yes"
} | sudo -u postgres -H tee --append ${data_dir}/postgresql.conf
sudo -u postgres -H \
     $(pg_config --bindir)/pg_ctl start \
     --pgdata=${data_dir}

cp -a \
   /host/sql \
   /host/expected \
   /tmp/
cd /tmp
ruby /host/test/prepare.rb > schedule
PG_REGRESS_DIFF_OPTS="-u"
if diff --help | grep -q color; then
  PG_REGRESS_DIFF_OPTS="${PG_REGRESS_DIFF_OPTS} --color=always"
fi
export PG_REGRESS_DIFF_OPTS
pg_regress=$(dirname $(pg_config --pgxs))/../test/regress/pg_regress

echo "::endgroup::"

run_test

echo "::group::Upgrade"

apt purge -V -y ${pgroonga_package}

if apt show ${pgroonga_package} > /dev/null 2>&1; then
  is_first_release=no
else
  is_first_release=yes
fi

if [ "${is_first_release}" = "yes" ]; then
  echo "Skip because ${pgroonga_package} hasn't been released yet."
else
  pgroonga_latest_released_version_full=$(apt info ${pgroonga_package} | \
                                            grep Version | \
                                            cut -d' ' -f2)
  pgroonga_latest_released_version=$(echo ${pgroonga_latest_released_version_full} | \
                                       cut -d'-' -f1)

  apt install -V -y ${pgroonga_package}
  createdb upgrade
  psql upgrade -c 'CREATE EXTENSION pgroonga'
  apt install -V -y \
    ${repositories_dir}/${os}/pool/${code_name}/*/*/*/*_${architecture}.deb
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
  apt install -V -y --allow-downgrades \
    ${pgroonga_package}=${pgroonga_latest_released_version_full}
fi

echo "::endgroup::"

echo "::group::Postpare"

sudo -u postgres -H \
     $(pg_config --bindir)/pg_ctl stop \
     --pgdata=${data_dir}

echo "::endgroup::"
