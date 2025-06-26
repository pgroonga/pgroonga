ARG FROM=ubuntu:jammy
FROM ${FROM}

RUN \
  echo "debconf debconf/frontend select Noninteractive" | \
    debconf-set-selections

ARG DEBUG
ARG POSTGRESQL_VERSION

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
    libxxhash-dev \
    pkg-config \
    postgresql-server-dev-${POSTGRESQL_VERSION} && \
  apt clean && \
  rm -rf /var/lib/apt/lists/*

RUN { \
      echo "DEBUILD_LINTIAN=no"; \
    } > ~/.devscripts
