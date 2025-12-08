---
name: Release
about: Issue for release
title: PGroonga X.Y.Z
labels: ''
assignees: ''
---

See https://pgroonga.github.io/development/release.html for details.

- [ ] Add a release note: https://github.com/pgroonga/pgroonga.github.io/blob/-/news/index.md
- [ ] Update compatibility list: `cd pgroogna.github.org && rake release:upgrade:update`
- [ ] Execute release task: `rake release`
- [ ] Prepare announcement text
  - [ ] Announce
    - [ ] X (Japanese/English)
- [ ] Check CI https://github.com/pgroonga/pgroonga/actions
- [ ] Check Ubuntu build https://launchpad.net/~groonga/+archive/ubuntu/nightly/+packages
- [ ] Blog: `cd groonga.org && rake pgroonga:release:blog:generate`
- [ ] Update Docker image: `cd pgroonga-docker && ./update.sh PGROONGA_VERSION GROONGA_VERSION`
- [ ] Announce
  - [ ] X (Japanese/English)
  - [ ] PostgreSQL Announce (pgsql-announce@lists.postgresql.org)
