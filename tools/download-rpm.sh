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
ARCH=$(rpm -qf --queryformat="%{ARCH}" /etc/redhat-release)
POSTGRESQL_MAJOR_VERSION=${POSTGRESQL_VERSION%%.*}

if [ ${OS_VERSION} -lt 8 ]; then
  sudo yum install -y yum-utils
  DNF_DOWNLOAD="yumdownloader"
  DNF_REPOQUERY_DEPLIST="yum deplist"
else
  sudo dnf install -y 'dnf-command(download)'
  DNF_DOWNLOAD="dnf download -y"
  DNF_REPOQUERY_DEPLIST="dnf repoquery --deplist"
fi

list_dependencies()
{
  local target="$1"
  yum deplist "${target}" | \
    grep provider: | \
    sed -e 's/^ *provider: //g' | \
    grep "\\.${ARCH}" | \
    sed -e "s/\\.${ARCH} /-/g" \
        -e "s/\\.${ARCH}$//g" | \
    sort | \
    uniq
}

system_libraries=(
  bash
  glibc
  krb5-libs
  libcurl
  libgcc
  libstdc++
  ncurses-libs
  openssl-libs
  postgresql${POSTGRESQL_MAJOR_VERSION}-server
)

is_system_library()
{
  local target="$1"

  for system_library in "${system_libraries[@]}"; do
    case "${target}" in
      ${system_library}-*)
        return 0
        ;;
    esac
  done

  return 1
}

processed_dependencies=()

download_recursive()
{
  local target="$1"

  echo "Downloading: ${target}"
  ${DNF_DOWNLOAD} "${target}.${ARCH}"
  processed_dependencies+=("${target}")

  for dependency in $(list_dependencies "${target}"); do
    echo "Dependency: ${target} -> ${dependency}"

    if is_system_library "${dependency}"; then
      continue
    fi

    # Pin Groonga version
    case "${dependency}" in
      groonga-libs-*)
        dependency="groonga-libs-${GROONGA_VERSION}-1.el${OS_VERSION}"
        ;;
    esac

    local processed="false"
    for processed_dependency in "${processed_dependencies[@]}"; do
      if [ "${processed_dependency}" = "${dependency}" ]; then
        processed="true"
        break
      fi
    done
    if [ "${processed}" = "true" ]; then
      continue
    fi

    download_recursive "${dependency}"
  done
}

download_recursive \
  postgresql${POSTGRESQL_MAJOR_VERSION}-pgdg-pgroonga-${PGROONGA_VERSION}-1.el${OS_VERSION}

if [ ${USE_MECAB:-no} = "yes" ]; then
  download_recursive \
    groonga-tokenizer-mecab-${GROONGA_VERSION}-1.el${OS_VERSION}
fi
