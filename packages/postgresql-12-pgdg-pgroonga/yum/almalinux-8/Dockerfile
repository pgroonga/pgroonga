ARG FROM=almalinux:8
FROM ${FROM}

ARG DEBUG

RUN <<EOF cat > /etc/yum.repos.d/pgdg-12.repo
[pgdg12-archive]
name=PostgreSQL 12 RPMs for RHEL/Rocky Linux/AlmaLinux 8
baseurl=https://yum-archive.postgresql.org/12/redhat/rhel-8-x86_64
enabled=1
gpgcheck=1
gpgkey=https://yum.postgresql.org/keys/PGDG-RPM-GPG-KEY-RHEL
EOF

RUN \
  quiet=$([ "${DEBUG}" = "yes" ] || echo "--quiet") && \
  dnf install -y ${quiet} \
    https://packages.apache.org/artifactory/arrow/almalinux/8/apache-arrow-release-latest.rpm \
    https://packages.groonga.org/almalinux/8/groonga-release-latest.noarch.rpm \
    epel-release && \
  dnf module -y ${quiet} disable postgresql && \
  dnf groupinstall -y ${quiet} "Development Tools" && \
  dnf install --enablerepo=powertools -y ${quiet} \
    ccache \
    # Normally, both arrow-devel and arrow-libs are installed automatically as dependency of groonga-devel.
    # However, when installed this way, the latest version(24.0.0) is also installed even if we pin groonga-devel to 14.0.6.
    # We want to restrict their versions to 16.1.0 to ensure compatibility.
    # We can install them at version 16.1.0 by pinning them to 16.1.0 when the packages are installed.
    arrow-devel-16.1.0-1.el8.x86_64 \
    arrow16-libs-16.1.0-1.el8.x86_64 \
    # If we do not pin the version of groonga-devel, groonga-devel uses the latest available version(16.0.5).
    # However, this branch uses PGroonga version 3.2.2, which was built with Groonga 14.0.6.
    # Therefore, we pin groonga-devel to 14.0.6 to ensure compatibility.
    groonga-devel-14.0.6-1.el8.x86_64 \
    llvm-toolset \
    llvm-devel \
    msgpack-devel \
    # If we do not pin the version of PostgreSQL, PostgreSQL uses the latest available version(12.22).
    # However, this branch uses PGroonga version 3.2.2, which was built with PostgreSQL 12.21.
    # Therefore, we pin PostgreSQL to 12.21 to ensure compatibility.
    postgresql12-devel-12.21-1PGDG.rhel8.x86_64 \
    xxhash-devel && \
  dnf clean -y ${quiet} all
