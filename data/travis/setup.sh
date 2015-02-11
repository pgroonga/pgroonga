#!/bin/sh

# set -x
set -e

curl --silent --location \
  https://github.com/groonga/groonga/raw/master/data/travis/setup.sh | \
  sh

pg_version=$(ruby -r yaml \
  -e 'print((YAML.load(ARGF.read)["addons"] || {})["postgresql"])' \
  .travis.yml)

if [ -n "${pg_version}" ]; then
  sudo apt-get install -qq -y postgresql-server-dev-${pg_version}
  git clone --recursive --depth 1 https://github.com/pgroonga/pgroonga.git
  cd pgroonga
  make > /dev/null
  sudo make install > /dev/null
  cd ..
fi
