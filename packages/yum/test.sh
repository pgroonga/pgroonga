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
    postgresql_package_prefix=$(rpm -qa | \
                                  grep pgroonga | \
                                  grep -E -o '^postgresql[0-9.]+' | \
                                  sed -e 's/\.//g')
    ${DNF} install -y ${postgresql_package_prefix}-devel
    pg_config=$(echo /usr/pgsql-*/bin/pg_config)
    ;;
  *)
    ${DNF} install -y postgresql-devel
    pg_config=pg_config
    ;;
esac

${DNF} install -y \
       diffutils \
       groonga-token-filter-stem \
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
