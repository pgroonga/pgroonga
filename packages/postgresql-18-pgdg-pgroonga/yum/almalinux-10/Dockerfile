ARG FROM=almalinux:10
FROM ${FROM}

ARG DEBUG
ARG POSTGRESQL_VERSION

RUN \
  quiet=$([ "${DEBUG}" = "yes" ] || echo "--quiet") && \
  dnf install -y ${quiet} \
    https://download.postgresql.org/pub/repos/yum/reporpms/EL-10-x86_64/pgdg-redhat-repo-latest.noarch.rpm \
    https://packages.apache.org/artifactory/arrow/almalinux/10/apache-arrow-release-latest.rpm \
    https://packages.groonga.org/almalinux/10/groonga-release-latest.noarch.rpm && \
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
