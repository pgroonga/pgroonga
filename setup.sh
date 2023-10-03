#!/bin/bash

set -eux

source_dir="$(dirname "$0")"

if type sudo > /dev/null 2>&1; then
  SUDO=sudo
else
  SUDO=
fi

if [ -f /etc/debian_version ]; then
  ${SUDO} apt update
  ${SUDO} apt install -y -V lsb-release wget
fi

if type lsb_release  > /dev/null 2>&1; then
  distribution=$(lsb_release --id --short | tr 'A-Z' 'a-z')
  code_name=$(lsb_release --codename --short)
else
  distribution=unknown
  code_name=unknown
fi

case "${distribution}-${code_name}" in
  debian-*)
    wget https://apache.jfrog.io/artifactory/arrow/${distribution}/apache-arrow-apt-source-latest-${code_name}.deb
    sudo apt install -y -V ./apache-arrow-apt-source-latest-${code_name}.deb
    wget https://packages.groonga.org/${distribution}/groonga-apt-source-latest-${code_name}.deb
    sudo apt install -y -V ./groonga-apt-source-latest-${code_name}.deb
    ;;
  ubuntu-*)
    sudo apt install -y -V software-properties-common
    sudo add-apt-repository -y universe
    sudo add-apt-repository -y ppa:groonga/ppa
    ;;
esac

case "${distribution}-${code_name}" in
  debian-*|ubuntu-*)
    sudo apt install -y -V gpg
    wget -O - https://www.postgresql.org/media/keys/ACCC4CF8.asc | \
      sudo gpg \
           --no-default-keyring \
           --keyring /usr/share/keyrings/pgdg.gpg \
           --import -
    (echo "Types: deb"; \
     echo "URIs: http://apt.postgresql.org/pub/repos/apt"; \
     echo "Suites: $(lsb_release --codename --short)-pgdg"; \
     echo "Components: main"; \
     echo "Signed-By: /usr/share/keyrings/pgdg.gpg") | \
      sudo tee /etc/apt/sources.list.d/pgdg.sources
    sudo apt update
    latest_postgresql_version=$(cd "${source_dir}/packages" && \
                                  echo postgresql-*-pgdg-pgroonga | \
                                    grep -o '[0-9]*' | \
                                    sort | \
                                    tail -n 1)
    sudo apt install -y -V \
         groonga-token-filter-stem \
         groonga-tokenizer-mecab \
         libgroonga-dev \
         postgresql-server-dev-${latest_postgresql_version}
    ;;
esac
