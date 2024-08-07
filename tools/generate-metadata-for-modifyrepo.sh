#!/bin/bash

# This script is used when building a local repository of RPMs.
# For modular packages, metadata for them is required.
# Generate the YAML required when registering that metadata with `modifyrepo_c` command.

set -eu

TARGET_RPM_PREFIXS=(mecab)
RPM_FILES_DIRECTORY=${1:-.}

cat <<YAML
---
document: modulemd-defaults
version: 1
data:
  module: pgroonga
  stream: "rolling"
...
---
document: modulemd
version: 2
data:
  name: pgroonga
  stream: "rolling"
  version: $(date "+%Y%m%d")
  summary: "pgroonga local repository module"
  description: "pgroonga local repository module"
  license:
    module:
    - LGPL-2.0-or-later
  artifacts:
    rpms:
YAML

for prefix in "${TARGET_RPM_PREFIXS[@]}"; do
  for rpm_file in $(find ${RPM_FILES_DIRECTORY} -name "${prefix}*.rpm"); do
    echo "    - $(basename ${rpm_file} | sed 's/.rpm$//')"
  done
done

cat <<YAML
...
YAML
