#!/bin/bash

set -eu

if [ $# -ne 1 ] && [ $# -ne 2 ]; then
  echo "USAGE: ./update-pgroonga-upgrade-page.sh compatible"
  echo "or"
  echo "USAGE: ./update-pgroonga-upgrade-page.sh incompatible \"a reason of incompatible\""
  exit 1
fi

COMPATIBLE_OR_NOT=$1

LATEST_VERSION=$(git tag --sort version:refname | tail -n 2 | sed -n '2p')
PREVIOUS_VERSION=$(git tag --sort version:refname | tail -n 2 | sed -n '1p')

git clone git@github.com:pgroonga/pgroonga.github.io.git
pushd pgroonga.github.io

if [ $COMPATIBLE_OR_NOT = "compatible" ]; then
  cat <<CONTENTS | sed -ie "/^Here is a list of compatibility/r /dev/stdin" upgrade/index.md

  * $PREVIOUS_VERSION -> $LATEST_VERSION: o
CONTENTS
else
  set +u
  INCOMPATIBLE_REASON=$2
  set -u
  cat <<CONTENTS | sed -ie "/^Here is a list of compatibility/r /dev/stdin" upgrade/index.md

  * $PREVIOUS_VERSION -> $LATEST_VERSION: x

    * $INCOMPATIBLE_REASON
CONTENTS
fi

# TODO: I'll remove translation.
# Because contents of this page doesn't need to translate.
rake jekyll:i18n:translate
git add _po/ja/upgrade/index.po
git add ja/upgrade/index.md
git add upgrade/index.md

git commit -m "upgrade: add $PREVIOUS_VERSION -> $LATEST_VERSION"
git push

popd
rm -rf pgroonga.github.io
