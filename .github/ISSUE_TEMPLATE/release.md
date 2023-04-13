---
name: Release
about: Issue for release
title: PGroonga x.x.x
labels: ''
assignees: ''

---

- [ ] NEWS https://github.com/pgroonga/pgroonga.github.io/blob/-r/news/index.md
- [ ] Prepare announcement text
  - [ ] Blog https://github.com/groonga/groonga.org
    - [ ] English
    - [ ] Japanese
  - [ ] Announce
    - [ ] Twitter (Japanese/English)
    - [ ] Facebook (Japanese/English)
- [ ] Check CI https://github.com/pgroonga/pgroonga/actions
- [ ] Check Ubuntu build https://launchpad.net/~groonga/+archive/ubuntu/nightly/+packages
- [ ] Tagging
- [ ] Upload source archives (.tar.gz、.zip) `rake package:source`
- [ ] Upload packages
  - [ ] Debian/Ubuntu `rake package:apt` -> `cd packages.groonga.org; rake apt`
  - [ ] Ubuntu (launchpad) `rake package:ubuntu`
  - [ ] AlmaLinux/CentOS `rake package:yum` -> `cd packages.groonga.org; rake yum`
- [ ] Blog
  - [ ] English
  - [ ] Japanese
- [ ] Announce
  - [ ] Twitter (Japanese/English)
  - [ ] Facebook (Japanese/English)
  - [ ] PostgreSQL Announce (pgsql-announce@lists.postgresql.org)
- [ ] Update Docker image
- [ ] Bump version for next release
