ARG FROM=ubuntu:focal
ARG POSTGRESQL_VERSION
FROM ${FROM}

RUN \
  echo "debconf debconf/frontend select Noninteractive" | \
    debconf-set-selections

ARG DEBUG

RUN \
  quiet=$([ "${DEBUG}" = "yes" ] || echo "-qq") && \
  apt update ${quiet} && \
  apt install -y -V ${quiet} software-properties-common && \
  add-apt-repository -y universe && \
  add-apt-repository -y ppa:groonga/ppa && \
  apt update ${quiet} && \
  apt install -y -V ${quiet} \
    build-essential \
    ccache \
    debhelper \
    devscripts \
    libgroonga-dev \
    libmsgpack-dev \
    pkg-config \
    postgresql-server-dev-${POSTGRESQL_VERSION} && \
  apt clean && \
  rm -rf /var/lib/apt/lists/*

RUN { \
      echo "DEBUILD_LINTIAN=no"; \
    } > ~/.devscripts
