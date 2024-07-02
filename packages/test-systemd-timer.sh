#!/usr/bin/env bash

set -xueo pipefail

# `pgroonga.enable_wal=yes` setting is required.

DBNAME=pgroonga_test
MEMOS_INDEX_NAME=memos_index
NOTES_INDEX_NAME=notes_index
SERVICE_PATH=/lib/systemd/system/pgroonga-primary-maintainer.service
TIMER_PATH=/lib/systemd/system/pgroonga-primary-maintainer.timer

pg_config=${1:-pg_config}
bindir=$(${pg_config} --bindir)

sudo -u postgres -H psql --command "CREATE DATABASE ${DBNAME};"
sudo -u postgres -H psql --dbname "${DBNAME}" --command "CREATE extension pgroonga";

sudo -u postgres -H psql --dbname "${DBNAME}" \
  --command "CREATE TABLE memos (content text);" \
  --command "CREATE INDEX ${MEMOS_INDEX_NAME} ON memos USING pgroonga (content);" \
  --command "CREATE TABLE notes (content text);" \
  --command "CREATE INDEX ${NOTES_INDEX_NAME} ON notes USING pgroonga (content);"

sudo -u postgres -H "${bindir}/pgroonga-generate-primary-maintainer-service.sh" \
  --pgroonga-primary-maintainer-command "${bindir}/pgroonga-primary-maintainer.sh" \
  --threshold 20K \
  --psql "${bindir}/psql" \
  --environment "PGDATABASE=${DBNAME}" | \
  tee "${SERVICE_PATH}"

sudo -u postgres -H "${bindir}/pgroonga-generate-primary-maintainer-timer.sh" \
  --time 1:00 | \
  sed 's/1:00:00/*:*:*/' | \
  tee "${TIMER_PATH}"

systemctl enable --now pgroonga-primary-maintainer.timer

function run_update_sqls (){
  sudo -u postgres -H psql --dbname "${DBNAME}" \
    --command "INSERT INTO memos SELECT 'PGroonga' FROM generate_series(1, 200);" \
    --command "DELETE FROM memos;" \
    --command "INSERT INTO notes SELECT 'NOTES' FROM generate_series(1, 200);" \
    --command "UPDATE notes SET content = 'NOTES-new';"
}

function wal_last_block () {
  index_name="${1}"
  sudo -u postgres -H psql --dbname "${DBNAME}" \
    --tuples-only \
    --command "SELECT last_block FROM pgroonga_wal_status() WHERE name = '${index_name}'"
}

ok=0
for i in {1..10}; do
  run_update_sqls
  before_memos_last_block=$(wal_last_block "${MEMOS_INDEX_NAME}")
  before_notes_last_block=$(wal_last_block "${NOTES_INDEX_NAME}")

  for retry in {1..10}; do
    after_memos_last_block=$(wal_last_block "${MEMOS_INDEX_NAME}")
    after_notes_last_block=$(wal_last_block "${NOTES_INDEX_NAME}")

    if [ ${before_memos_last_block} -gt ${after_memos_last_block} ] && \
      [ ${before_notes_last_block} -gt ${after_notes_last_block} ]; then
      ok=1
      break
    fi
    sleep 1
  done
  if [ ${ok} -ne 1 ]; then
    break
  fi
done

systemctl disable --now pgroonga-primary-maintainer.timer
rm -f "${SERVICE_PATH}" "${TIMER_PATH}"

sudo -u postgres -H psql --command "DROP DATABASE ${DBNAME};"

if [ ${ok} -eq 1 ]; then
  echo "OK."
else
  echo "Failed."
  exit 1
fi
