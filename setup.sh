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
  debian-*|ubuntu-*)
    wget https://packages.groonga.org/${distribution}/groonga-apt-source-latest-${code_name}.deb
    ${SUDO} apt install -y -V ./groonga-apt-source-latest-${code_name}.deb
    ;;
esac

case "${distribution}-${code_name}" in
  debian-*|ubuntu-*)
    ${SUDO} apt install -y -V gpg
    wget -O - https://www.postgresql.org/media/keys/ACCC4CF8.asc | \
      ${SUDO} gpg \
           --no-default-keyring \
           --keyring /usr/share/keyrings/pgdg.gpg \
           --import -
    (echo "Types: deb"; \
     echo "URIs: http://apt.postgresql.org/pub/repos/apt"; \
     echo "Suites: $(lsb_release --codename --short)-pgdg"; \
     echo "Components: main"; \
     echo "Signed-By: /usr/share/keyrings/pgdg.gpg") | \
      ${SUDO} tee /etc/apt/sources.list.d/pgdg.sources
    ${SUDO} apt update
    latest_postgresql_version=$(cd "${source_dir}/packages" && \
                                  echo postgresql-*-pgdg-pgroonga | \
                                    grep -o '[0-9]*' | \
                                    sort | \
                                    tail -n 1)
    ${SUDO} apt install -y -V \
         gcc \
         groonga-token-filter-stem \
         groonga-tokenizer-mecab \
         libgroonga-dev \
         libmsgpack-dev \
         libxxhash-dev \
         meson \
         ninja-build \
         postgresql-${latest_postgresql_version} \
         postgresql-server-dev-${latest_postgresql_version} \
         ruby
    ;;
esac
