#!/bin/bash

set -ue

PGROONGA_LATEST_VERSION_AND_DATE=$(curl https://api.github.com/repos/pgroonga/pgroonga/releases/latest | jq -r '.name')
PGROONGA_LATEST_VERSION=$(curl https://api.github.com/repos/pgroonga/pgroonga/releases/latest | jq -r '.tag_name')

RELEASE_BLOG_BASE_URL="https://groonga.org"
RELEASE_BLOG_DATE_PATH=$(echo $PGROONGA_LATEST_VERSION_AND_DATE | cut -d ' ' -f 3 | tr - /)
RELEASE_BLOG_VERSION="pgroonga-$PGROONGA_LATEST_VERSION"

RELEASE_BLOG_URL_JA="$RELEASE_BLOG_BASE_URL/ja/blog/$RELEASE_BLOG_DATE_PATH/$RELEASE_BLOG_VERSION.html"
RELEASE_BLOG_URL_EN="$RELEASE_BLOG_BASE_URL/en/blog/$RELEASE_BLOG_DATE_PATH/$RELEASE_BLOG_VERSION.html"

HTTP_RESPONCE_CODE_JA=$(curl --head $RELEASE_BLOG_URL_JA | head -n 1 | cut -d ' ' -f 2)
HTTP_RESPONCE_CODE_EN=$(curl --head $RELEASE_BLOG_URL_EN | head -n 1 | cut -d ' ' -f 2)

if [ $HTTP_RESPONCE_CODE_JA -eq 404 ]; then
  echo "$RELEASE_BLOG_URL_JA is not found. This URL is incorrect."
  exit 1
elif [ $HTTP_RESPONCE_CODE_EN -eq 404 ]; then
  echo "$RELEASE_BLOG_URL_EN is not found. This URL is incorrect."
  exit 1
fi

ANNOUNCE_JA_TEMPLATE="$PGROONGA_LATEST_VERSION_AND_DATE リリース！ $RELEASE_BLOG_URL_JA 今回のリリースでは、"
ANNOUNCE_EN_TEMPLATE="$PGROONGA_LATEST_VERSION_AND_DATE has been released! $RELEASE_BLOG_URL_EN In this release,"

echo $ANNOUNCE_JA_TEMPLATE; echo $ANNOUNCE_EN_TEMPLATE
