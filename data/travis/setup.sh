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

  if [ "${PGROONGA_MASTER}" = "yes" ]; then
    git clone --quiet --recursive --depth 1 \
      https://github.com/pgroonga/pgroonga.git
    cd pgroonga
  else
    curl --silent --location --remote-name --fail \
      http://packages.groonga.org/source/pgroonga/pgroonga-latest.tar.gz
    tar xf pgroonga-*.tar.gz
    rm pgroonga-*.tar.gz
    cd pgroonga-*/
  fi
  make > /dev/null
  sudo make install > /dev/null
  cd ..

  postgresql -U postgres -d template1 -c 'CREATE EXTENSION pgroonga;'
fi
