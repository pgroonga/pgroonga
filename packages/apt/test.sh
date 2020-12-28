#!/bin/bash

set -eux

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

apt update

apt install -V -y \
  ${repositories_dir}/${os}/pool/${code_name}/*/*/*/*_${architecture}.deb

postgresql_package_prefix=$(dpkg -l | \
                              grep pgroonga | \
                              grep -E -o 'postgresql-[0-9.]+(-pgdg)?' |
                              head -n1)
if ! echo "${postgresql_package_prefix}" | grep -q pdgd; then
  apt install -V -y \
      $(echo ${postgresql_package_prefix} | \
          sed -e 's/^postgresql-/postgresql-server-dev-/')
fi

apt install -V -y \
    groonga-token-filter-stem \
    groonga-tokenizer-mecab \
    ruby \
    sudo

data_dir=/tmp/data
sudo -u postgres -H \
     $(pg_config --bindir)/initdb \
     --encoding=UTF-8 \
     --locale=C \
     --pgdata=${data_dir} \
     --username=root
sudo -u postgres -H \
     $(pg_config --bindir)/pg_ctl start \
     --pgdata=/${data_dir}

cp -a \
   /host/sql \
   /host/expected \
   /tmp/
cd /tmp
ruby /host/test/prepare.rb > schedule
export PG_REGRESS_DIFF_OPTS="-u --color=always"
pg_regress=$(dirname $(pg_config --pgxs))/../test/regress/pg_regress
set +e
${pg_regress} \
  --launcher=/host/test/short-pgappname \
  --load-extension=pgroonga \
  --schedule=schedule
pg_regress_status=$?
set -e
if [ ${pg_regress_status} -ne 0 ]; then
  cat regression.diffs
  exit ${pg_regress_status}
fi

sudo -u postgres -H \
     $(pg_config --bindir)/pg_ctl stop \
     --pgdata=/${data_dir}
