#!/bin/bash

# This script is used when building a local repository of RPMs.
# For modular packages, metadata for them is required.
# Generate the YAML required when registering that metadata with `modifyrepo_c` command.

set -eu

RPM_FILES_DIRECTORY=${1:-.}

cat <<YAML
---
document: modulemd-defaults
version: 1
data:
  module: mecab
  stream: "rolling"
...
---
document: modulemd
version: 2
data:
  name: mecab
  stream: "rolling"
  version: $(date "+%Y%m%d")
  summary: "mecab module in pgroonga local repository"
  description: "mecab module in pgroonga local repository"
  license:
    module:
    - GPL-2.0-or-later
    - LGPL-2.1-or-later
    - BSD-3-Clause
  artifacts:
    rpms:
YAML

for rpm_file in $(find ${RPM_FILES_DIRECTORY} -name "mecab*.rpm"); do
  echo "    - $(basename ${rpm_file} | sed 's/.rpm$//')"
done

cat <<YAML
...
YAML
