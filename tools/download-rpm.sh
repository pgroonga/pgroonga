#!/bin/bash

set -eu

if [ $# != 3 ]; then
  echo "Usage: $0 PGROONGA_VERSION GROOGNA_VERSION POSTGRESQL_VERSION"
  echo " e.g.: $0 2.4.6 13.0.0 15.2"
  exit 1
fi

PGROONGA_VERSION=$1
GROONGA_VERSION=$2
POSTGRESQL_VERSION=$3

OS_VERSION=$(cut -d: -f5 /etc/system-release-cpe | cut -d. -f1)
POSTGRESQL_MAJOR_VERSION=${POSTGRESQL_VERSION%%.*}

if [ ${OS_VERSION} -lt 8 ]; then
  sudo yum install -y yum-utils
  DNF_DOWNLOAD="yumdownloader"
  DNF_INFO="yum info"
else
  sudo dnf install -y 'dnf-command(download)'
  DNF_DOWNLOAD="dnf download --arch x86_64"
fi

list_dependencies()
{
  local target=$1
  if [ ${OS_VERSION} -lt 8 ]; then
    yum deplist ${target} | \
      grep provider: | \
      sed -e 's/^ *provider: //g' | \
      awk '{print $1}' | \
      sed -e '/i686$/d' | \
      sort | uniq
  else
    :
  fi
}

# TODO: Use this to prevent infinite loop
processed=()

download_recursive()
{
  local target=$1

  ${DNF_DOWNLOAD} ${target}

  list_dependencies ${target} | while read dependency; do
    installed=$(${DNF_INFO} ${dependency} | \
      grep Repo | \
      sed -e 's/^Repo *: //g')
    if [[ ${installed} =~ "installed" ]]; then
      continue
    fi
#    if [[ ${dependency} =~ postgresql ]]; then
#      POSTGRESQL_MEJOR_VERSION=$(awk -F "." '{print $1}')
#      if [[ ! ${dependency} =~ "postgresql${POSTGRESQL_MEJOR_VERSION}" ]]; then
#        continue
#      fi
#    fi
    download_recursive ${dependency}
  done
}

download_recursive \
  postgresql${POSTGRESQL_MAJOR_VERSION}-pgdg-pgroonga-${PGROONGA_VERSION}-1.el${OS_VERSION}
