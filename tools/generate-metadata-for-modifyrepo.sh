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
  summary: "MeCab module in PGroonga local repository"
  description: "MeCab module in PGroonga local repository"
  license:
    module:
    - GPL-2.0-or-later
    - LGPL-2.1-or-later
    - BSD-3-Clause
  artifacts:
    rpms:
YAML

for rpm_file in $(find ${RPM_FILES_DIRECTORY} -name "mecab*.rpm"); do
  # Convert
  # rpm_file (mecab-0.996-2.module_el8.6.0+3340+d764b636.x86_64.rpm)
  # to
  # artifact (mecab-0:0.996-2.module_el8.6.0+3340+d764b636.x86_64)
  #
  # Add `0:` before the version.

  package_name=$(rpm -qp --queryformat '%{NAME}' ${rpm_file})
  artifact=$(basename ${rpm_file} | sed -E -e 's/.rpm$//' -e "s/(${package_name})-/\1-0:/")
  echo "    - ${artifact}"
done

cat <<YAML
...
YAML
