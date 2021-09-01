#!/bin/bash

set -eux

os=$(cut -d: -f4 /etc/system-release-cpe)
version=$(cut -d: -f5 /etc/system-release-cpe)
case ${os} in
  centos)
    case ${version} in
      7)
        DNF=yum
        ;;
      *)
        DNF="dnf --enablerepo=powertools"
        ${DNF} module -y disable postgresql
        ;;
    esac

    ${DNF} install -y \
           https://download.postgresql.org/pub/repos/yum/reporpms/EL-${version}-x86_64/pgdg-redhat-repo-latest.noarch.rpm \
           https://packages.groonga.org/centos/groonga-release-latest.noarch.rpm
    ;;
  fedora)
    DNF="dnf"
    ;;
esac

repositories_dir=/host/repositories
${DNF} install -y \
       ${repositories_dir}/${os}/${version}/x86_64/Packages/*.rpm

case ${os} in
  centos)
    case ${version} in
      7)
        ${DNF} install -y centos-release-scl
        ;;
    esac
    postgresql_version=$(rpm -qa | \
                           grep pgroonga | \
                           grep -E -o '[0-9.]+' |
                           head -n1)
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
       diffutils \
       groonga-tokenizer-mecab \
       ruby \
       sudo

data_dir=/tmp/data
sudo -u postgres -H \
     $(${pg_config} --bindir)/initdb \
     --encoding=UTF-8 \
     --locale=C \
     --pgdata=${data_dir} \
     --username=root
sudo -u postgres -H \
     $(${pg_config} --bindir)/pg_ctl start \
     --pgdata=/${data_dir}

cp -a \
   /host/sql \
   /host/expected \
   /tmp/
cd /tmp
case "${os}" in
  centos)
    if [ "$((${postgresql_version} < 13))" = 1 ]; then
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
export PG_REGRESS_DIFF_OPTS="-u --color=always"
pg_regress=$(dirname $(${pg_config} --pgxs))/../test/regress/pg_regress
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
     $(${pg_config} --bindir)/pg_ctl stop \
     --pgdata=/${data_dir}
