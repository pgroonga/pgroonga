ARG FROM=almalinux:9
FROM ${FROM}

ARG DEBUG
ARG POSTGRESQL_VERSION

RUN \
  quiet=$([ "${DEBUG}" = "yes" ] || echo "--quiet") && \
  dnf install -y ${quiet} \
    https://download.postgresql.org/pub/repos/yum/reporpms/EL-$(cut -d: -f5 /etc/system-release-cpe | cut -d. -f1)-x86_64/pgdg-redhat-repo-latest.noarch.rpm \
    https://packages.apache.org/artifactory/arrow/almalinux/$(cut -d: -f5 /etc/system-release-cpe | cut -d. -f1)/apache-arrow-release-latest.rpm \
    https://packages.groonga.org/almalinux/$(cut -d: -f5 /etc/system-release-cpe | cut -d. -f1)/groonga-release-latest.noarch.rpm && \
  sed -i'' -e 's/k$//g' /etc/yum.repos.d/pgdg-redhat-all.repo && \
  dnf groupinstall -y ${quiet} "Development Tools" && \
  dnf install --enablerepo=crb -y ${quiet} \
    ccache \
    groonga-devel \
    llvm-toolset \
    llvm-devel \
    meson \
    msgpack-devel \
    ninja-build \
    postgresql${POSTGRESQL_VERSION}-devel \
    xxhash-devel && \
  dnf clean -y ${quiet} all
