#!/bin/bash

sudo apt-get -V -y install wget tar build-essential zlib1g-dev liblzo2-dev libmsgpack-dev libevent-dev libmecab-dev libreadline-dev zlib1g-dev
sudo apt-get -y install software-properties-common
sudo add-apt-repository -y universe
sudo add-apt-repository -y ppa:groonga/ppa
sudo apt-get update

sudo apt-get -y install groonga
sudo apt-get -y install libgroonga-dev groonga-tokenizer-mecab groonga-token-filter-stem groonga-munin-plugins groonga-normalizer-mysql
