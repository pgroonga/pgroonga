#!/bin/bash

set -ue

PGROONGA_LATEST_VERSION=$(curl https://api.github.com/repos/pgroonga/pgroonga/releases/latest | jq -r '.tag_name')
PGROONGA_LATEST_VERSION_AND_DATE=$(curl https://api.github.com/repos/pgroonga/pgroonga/releases/latest | jq -r '.name')

PGROONGA_LATEST_RELEASE_DATE=$(echo $PGROONGA_LATEST_VERSION_AND_DATE | cut -d ' ' -f 3)
BLOG_EN_FILE_NAME=$PGROONGA_LATEST_RELEASE_DATE-pgroonga-$PGROONGA_LATEST_VERSION.md


rm -f $BLOG_EN_FILE_NAME

PGROONGA_BLOG_HEADER=$(cat <<HEADER
---
layout: post.en
title: PGroonga (fast full text search module for PostgreSQL) $PGROONGA_LATEST_VERSION has been released
description: PGroonga (fast full text search module for PostgreSQL) $PGROONGA_LATEST_VERSION has been released!
---
HEADER
                    )
echo "$PGROONGA_BLOG_HEADER" >> $BLOG_EN_FILE_NAME

PGROONGA_BLOG_TITLE=$(cat <<TITLE

## PGroonga (fast full text search module for PostgreSQL) $PGROONGA_LATEST_VERSION has been released

[PGroonga](https://pgroonga.github.io/) $PGROONGA_LATEST_VERSION has been released! PGroonga makes PostgreSQL fast full text search for all languages.
TITLE
                   )
echo "$PGROONGA_BLOG_TITLE" >> $BLOG_EN_FILE_NAME

PGROONGA_LATEST_RELEASE_NOTE=$(curl https://api.github.com/repos/pgroonga/pgroonga/releases/latest | jq -r '.body')
PGROONGA_BLOG_CONTENTS=$(echo "$PGROONGA_LATEST_RELEASE_NOTE" | sed '1d')

echo "$PGROONGA_BLOG_CONTENTS" >> $BLOG_EN_FILE_NAME

FOR_YOUR_INFORMATION=$(cat <<INFORMATION

### How to upgrade

If you're using PGroonga 2.0.0 or later, you can upgrade by steps in "Compatible case" in [Upgrade document](https://pgroonga.github.io/upgrade/#compatible-case).

If you're using PGroonga 1.Y.Z, you can upgrade by steps in "Incompatible case" in [Upgrade document](https://pgroonga.github.io/upgrade/#incompatible-case).

### Support service

If you need commercial support for PGroonga, [contact us](mailto:info@clear-code.com).

### Conclusion

Try PGroonga when you want to perform fast full text search against all languages on PostgreSQL!
INFORMATION
                    )

echo "$FOR_YOUR_INFORMATION" >> $BLOG_EN_FILE_NAME

