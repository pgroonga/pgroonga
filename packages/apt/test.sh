#!/bin/bash

set -eux

apt update
apt install -V -y \
    lsb-release \
    wget

code_name=$(lsb_release --codename --short)
architecture=$(dpkg --print-architecture)

repositories_dir=/pgroonga/repositories

wget \
  https://packages.groonga.org/debian/groonga-apt-source-latest-${code_name}.deb
apt install -V -y ./groonga-apt-source-latest-${code_name}.deb

if find ${repositories_dir} | grep -q "pgdg"; then
  echo "deb http://apt.postgresql.org/pub/repos/apt/ ${code_name}-pgdg main" | \
    tee /etc/apt/sources.list.d/pgdg.list
  wget --quiet -O - https://www.postgresql.org/media/keys/ACCC4CF8.asc | \
    apt-key add -
fi

apt update

apt install -V -y \
  ${repositories_dir}/debian/pool/${code_name}/main/*/*/*_${architecture}.deb

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
   /pgroonga/sql \
   /pgroonga/expected \
   /tmp/
cd /tmp
ruby /pgroonga/test/prepare.rb > schedule
export PG_REGRESS_DIFF_OPTS="-u --color=always"
pg_regress=$(dirname $(pg_config --pgxs))/../test/regress/pg_regress
set +e
${pg_regress} \
  --launcher=/pgroonga/test/short-pgappname \
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
