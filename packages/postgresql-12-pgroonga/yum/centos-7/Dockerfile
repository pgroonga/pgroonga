ARG FROM=centos:7
FROM ${FROM}

ARG DEBUG

RUN \
  quiet=$([ "${DEBUG}" = "yes" ] || echo "--quiet") && \
  yum install -y ${quiet} \
    https://download.postgresql.org/pub/repos/yum/reporpms/EL-7-x86_64/pgdg-redhat-repo-latest.noarch.rpm \
    https://packages.groonga.org/centos/groonga-release-latest.noarch.rpm \
    centos-release-scl \
    epel-release && \
  yum groupinstall -y ${quiet} "Development Tools" && \
  yum install -y ${quiet} \
    ccache \
    groonga-devel \
    llvm-toolset-7 \
    llvm5.0-devel \
    msgpack-devel \
    postgresql12-devel && \
  yum clean -y ${quiet} all
