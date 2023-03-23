#!/bin/bash

set -eux

version=$(cut -d: -f5 /etc/system-release-cpe | cut -d. -f1)

if [ ${version} -lt 8 ]; then
  sudo -H yum install -y \
       https://download.postgresql.org/pub/repos/yum/reporpms/EL-${version}-$(rpm -qf --queryformat="%{ARCH}" /etc/redhat-release)/pgdg-redhat-repo-latest.noarch.rpm
  sudo -H yum install -y \
       https://packages.groonga.org/centos/${version}/groonga-release-latest.noarch.rpm
else
  sudo -H dnf module -y disable postgresql
  sudo -H dnf install -y epel-release || \
    sudo -H dnf install -y \
         oracle-epel-release-el${version} || \
    sudo -H dnf install -y \
         https://dl.fedoraproject.org/pub/epel/epel-release-latest-${version}.noarch.rpm
  sudo -H dnf config-manager --set-enabled powertools || :
  sudo -H dnf install -y \
       https://download.postgresql.org/pub/repos/yum/reporpms/EL-${version}-$(rpm -qf --queryformat="%{ARCH}" /etc/redhat-release)/pgdg-redhat-repo-latest.noarch.rpm
  sudo -H dnf install -y \
       https://packages.groonga.org/almalinux/${version}/groonga-release-latest.noarch.rpm
fi
