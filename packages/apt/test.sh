#!/bin/bash

set -eux


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

wget \
  https://packages.groonga.org/${os}/groonga-apt-source-latest-${code_name}.deb
apt install -V -y ./groonga-apt-source-latest-${code_name}.deb

if [ "${os}" = "ubuntu" ]; then
  apt install -y software-properties-common
  add-apt-repository -y universe
  add-apt-repository -y ppa:groonga/ppa
fi

if find ${repositories_dir} | grep -q "pgdg"; then
  echo "deb http://apt.postgresql.org/pub/repos/apt/ ${code_name}-pgdg main" | \
    tee /etc/apt/sources.list.d/pgdg.list
  wget --quiet -O - https://www.postgresql.org/media/keys/ACCC4CF8.asc | \
    apt-key add -
fi

echo "::endgroup::"


echo "::group::Install built packages"

apt update

apt install -V -y \
  ${repositories_dir}/${os}/pool/${code_name}/*/*/*/*_${architecture}.deb

echo "::endgroup::"


echo "::group::Install packages for test"

postgresql_version=$(dpkg -l | \
                       grep pgroonga | \
                       grep -E -o '[0-9.]+' |
                       head -n1)
postgresql_package_prefix=$(dpkg -l | \
                              grep pgroonga | \
                              grep -E -o 'postgresql-[0-9.]+(-pgdg)?' |
                              head -n1)
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

data_dir=/tmp/data
sudo -u postgres -H \
     $(pg_config --bindir)/initdb \
     --encoding=UTF-8 \
     --locale=C \
     --pgdata=${data_dir} \
     --username=root
echo "max_prepared_transactions = 1" | \
  sudo -u postgres -H tee --append ${data_dir}/postgresql.conf
sudo -u postgres -H \
     $(pg_config --bindir)/pg_ctl start \
     --pgdata=${data_dir}

cp -a \
   /host/sql \
   /host/expected \
   /tmp/
cd /tmp
if [ "$((${postgresql_version} < 13))" -eq 1 ]; then
  rm sql/full-text-search/text/single/declarative-partitioning.sql
fi
ruby /host/test/prepare.rb > schedule
PG_REGRESS_DIFF_OPTS="-u"
if diff --help | grep -q color; then
  PG_REGRESS_DIFF_OPTS="${PG_REGRESS_DIFF_OPTS} --color=always"
fi
export PG_REGRESS_DIFF_OPTS
pg_regress=$(dirname $(pg_config --pgxs))/../test/regress/pg_regress

echo "::endgroup::"


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
  exit ${pg_regress_status}
fi


echo "::group::Postpare"

sudo -u postgres -H \
     $(pg_config --bindir)/pg_ctl stop \
     --pgdata=${data_dir}

echo "::endgroup::"
