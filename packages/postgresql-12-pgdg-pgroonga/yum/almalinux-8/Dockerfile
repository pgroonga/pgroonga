ARG FROM=almalinux:8
ARG POSTGRESQL_VERSION
FROM ${FROM}

ARG DEBUG

RUN \
  quiet=$([ "${DEBUG}" = "yes" ] || echo "--quiet") && \
  dnf install -y ${quiet} \
    https://download.postgresql.org/pub/repos/yum/reporpms/EL-8-x86_64/pgdg-redhat-repo-latest.noarch.rpm \
    https://packages.groonga.org/almalinux/8/groonga-release-latest.noarch.rpm \
    epel-release && \
  sed -i'' -e 's/k$//g' /etc/yum.repos.d/pgdg-redhat-all.repo && \
  dnf module -y ${quiet} disable postgresql && \
  dnf groupinstall -y ${quiet} "Development Tools" && \
  dnf install --enablerepo=powertools -y ${quiet} \
    ccache \
    groonga-devel \
    llvm-toolset \
    llvm-devel \
    msgpack-devel \
    postgresql${POSTGRESQL_VERSION}-devel \
    xxhash-devel && \
  dnf clean -y ${quiet} all
