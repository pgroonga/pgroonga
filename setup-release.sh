#!/bin/bash

set -eux

if type sudo > /dev/null 2>&1; then
  SUDO=sudo
else
  SUDO=
fi

if [ -f /etc/debian_version ]; then
  ${SUDO} apt update
  ${SUDO} apt install -y -V lsb-release
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
    ${SUDO} apt install -y -V \
         createrepo-c \
         debhelper \
         devscripts \
         gh \
         gnupg2 \
         rpm
    ;;
esac
