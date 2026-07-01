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
    groonga-devel-14.0.6-1.el8.x86_64 \
    llvm-toolset \
    llvm-devel \
    msgpack-devel \
    postgresql12-devel \
    xxhash-devel && \
  dnf clean -y ${quiet} all
