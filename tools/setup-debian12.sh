#!/bin/bash

sudo apt-get -V -y install wget tar build-essential zlib1g-dev liblzo2-dev libmsgpack-dev libevent-dev libmecab-dev libreadline-dev zlib1g-dev
sudo apt update
sudo apt install -y -V ca-certificates lsb-release wget
wget https://apache.jfrog.io/artifactory/arrow/$(lsb_release --id --short | tr 'A-Z' 'a-z')/apache-arrow-apt-source-latest-$(lsb_release --codename --short).deb
sudo apt install -y -V ./apache-arrow-apt-source-latest-$(lsb_release --codename --short).deb
sudo apt update
wget https://packages.groonga.org/debian/groonga-apt-source-latest-$(lsb_release --codename --short).deb
sudo apt install -y -V ./groonga-apt-source-latest-$(lsb_release --codename --short).deb
sudo apt update
sudo apt install -y -V groonga libgroonga-dev groonga-tokenizer-mecab groonga-token-filter-stem groonga-munin-plugins groonga-normalizer-mysql
