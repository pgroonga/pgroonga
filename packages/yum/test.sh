#!/bin/bash

set -eux


echo "::group::Prepare repositories"

os=$(cut -d: -f4 /etc/system-release-cpe)
major_version=$(cut -d: -f5 /etc/system-release-cpe | grep -o "^[0-9]")
case ${os} in
  amazon)
    os=amazon-linux
    version=$(cut -d: -f6 /etc/system-release-cpe)
    DNF=yum
    amazon-linux-extras install -y epel
    ${DNF} install -y ca-certificates
    ${DNF} install -y \
           https://packages.groonga.org/${os}/${major_version}/groonga-release-latest.noarch.rpm
    ;;
  almalinux|centos)
    case ${major_version} in
      7)
        DNF=yum
        ;;
      *)
        DNF="dnf --enablerepo=powertools"
        ${DNF} module -y disable postgresql
        ;;
    esac

    ${DNF} install -y \
           https://download.postgresql.org/pub/repos/yum/reporpms/EL-${major_version}-x86_64/pgdg-redhat-repo-latest.noarch.rpm \
           https://packages.groonga.org/${os}/${major_version}/groonga-release-latest.noarch.rpm
    ;;
  fedora)
    DNF="dnf"
    ;;
esac

echo "::endgroup::"


echo "::group::Install built packages"

repositories_dir=/host/repositories
${DNF} install -y \
       ${repositories_dir}/${os}/${major_version}/x86_64/Packages/*.rpm

echo "::endgroup::"


echo "::group::Install packages for test"

postgresql_version=$(rpm -qa | \
                       grep pgroonga | \
                       grep -E -o '[0-9.]+' |
                       head -n1)
case ${os} in
  amazon-linux)
    amazon-linux-extras install -y postgresql${postgresql_version}
    ${DNF} install -y \
           libpq-devel \
           postgresql-server-devel
    pg_config=pg_config
    groonga_token_filter_stem_package_name=groonga-token-filter-stem
    ;;
  almalinux|centos)
    case ${major_version} in
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
sudo -u postgres -H \
     $(${pg_config} --bindir)/pg_ctl start \
     --pgdata=${data_dir}

cp -a \
   /host/sql \
   /host/expected \
   /tmp/
cd /tmp
case "${os}" in
  almalinux|amazon-linux|centos)
    if [ "$((${postgresql_version} < 11))" -eq 1 ]; then
      rm sql/index-only-scan/include.sql
    fi
    if [ "$((${postgresql_version} < 13))" -eq 1 ]; then
      rm sql/full-text-search/text/single/declarative-partitioning.sql
    fi
    # TODO: PostgreSQL 14.3, 13.7, 12.11, 11.16 and 10.21 will fix
    # PostgreSQL bug of them.
    if [ "$((${postgresql_version} < 15))" -eq 1 ]; then
      rm sql/index-only-scan/count-star-large.sql
      rm sql/jsonb/count-star/indexscan.sql
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
     $(${pg_config} --bindir)/pg_ctl stop \
     --pgdata=${data_dir}

echo "::endgroup::"
