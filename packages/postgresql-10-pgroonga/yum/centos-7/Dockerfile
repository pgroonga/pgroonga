ARG FROM=centos:7
FROM ${FROM}

ARG DEBUG

RUN \
  quiet=$([ "${DEBUG}" = "yes" ] || echo "--quiet") && \
  yum install -y ${quiet} \
    https://download.postgresql.org/pub/repos/yum/reporpms/EL-7-x86_64/pgdg-redhat-repo-latest.noarch.rpm \
    https://packages.groonga.org/centos/7/groonga-release-latest.noarch.rpm \
    epel-release && \
  sed -i'' -e 's/k$//g' /etc/yum.repos.d/pgdg-redhat-all.repo && \
  yum groupinstall -y ${quiet} "Development Tools" && \
  yum install -y ${quiet} \
    ccache \
    groonga-devel \
    msgpack-devel \
    postgresql10-devel \
    xxhash-devel && \
  yum clean -y ${quiet} all
