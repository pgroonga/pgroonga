ARG FROM=almalinux:9
FROM ${FROM}

ARG DEBUG

RUN \
  quiet=$([ "${DEBUG}" = "yes" ] || echo "--quiet") && \
  dnf install -y ${quiet} \
    https://download.postgresql.org/pub/repos/yum/reporpms/EL-9-x86_64/pgdg-redhat-repo-latest.noarch.rpm \
    https://apache.jfrog.io/artifactory/arrow/almalinux/9/apache-arrow-release-latest.rpm \
    https://packages.groonga.org/almalinux/9/groonga-release-latest.noarch.rpm && \
  sed -i'' -e 's/k$//g' /etc/yum.repos.d/pgdg-redhat-all.repo && \
  dnf groupinstall -y ${quiet} "Development Tools" && \
  dnf install --enablerepo=crb -y ${quiet} \
    ccache \
    groonga-devel \
    llvm-toolset \
    llvm-devel \
    msgpack-devel \
    postgresql15-devel \
    xxhash-devel && \
  dnf clean -y ${quiet} all
