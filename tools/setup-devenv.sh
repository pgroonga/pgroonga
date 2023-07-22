#!/bin/bash

set -eux

if [ -f /etc/debian_version ]; then
  sudo apt install -y -V lsb-release
  distribution=$(lsb_release --id --short | tr 'A-Z' 'a-z')
  code_name=$(lsb_release --codename --short)
  case "${distribution}-${code_name}" in
    debian-bookworm)
      # For Debian 12
        sudo apt-get -V -y install wget tar build-essential zlib1g-dev liblzo2-dev libmsgpack-dev libevent-dev libmecab-dev libreadline-dev
        sudo apt update
        sudo apt install -y -V ca-certificates wget
        wget https://apache.jfrog.io/artifactory/arrow/$(lsb_release --id --short | tr 'A-Z' 'a-z')/apache-arrow-apt-source-latest-$(lsb_release --codename --short).deb
        sudo apt install -y -V ./apache-arrow-apt-source-latest-$(lsb_release --codename --short).deb
        sudo apt update
        wget https://packages.groonga.org/debian/groonga-apt-source-latest-$(lsb_release --codename --short).deb
        sudo apt install -y -V ./groonga-apt-source-latest-$(lsb_release --codename --short).deb
        sudo apt update
        sudo apt install -y -V groonga libgroonga-dev groonga-tokenizer-mecab groonga-token-filter-stem groonga-munin-plugins groonga-normalizer-mysql
      ;;
    ubuntu-jammy)
      # For Ubuntu 22.04
        sudo apt-get -V -y install wget tar build-essential zlib1g-dev liblzo2-dev libmsgpack-dev libevent-dev libmecab-dev libreadline-dev
        sudo apt-get -y install software-properties-common
        sudo add-apt-repository -y universe
        sudo add-apt-repository -y ppa:groonga/ppa
        sudo apt-get update

        sudo apt-get -y install groonga
        sudo apt-get -y install libgroonga-dev groonga-tokenizer-mecab groonga-token-filter-stem groonga-munin-plugins groonga-normalizer-mysql
      ;;
  esac
fi