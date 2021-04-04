ARG FROM=ubuntu:bionic
FROM ${FROM}

RUN \
  echo "debconf debconf/frontend select Noninteractive" | \
    debconf-set-selections

ARG DEBUG

RUN \
  quiet=$([ "${DEBUG}" = "yes" ] || echo "-qq") && \
  apt update ${quiet} && \
  apt install -y -V ${quiet} \
    gnupg \
    software-properties-common \
    wget && \
  add-apt-repository -y universe && \
  add-apt-repository -y ppa:groonga/ppa && \
  echo "deb http://apt.postgresql.org/pub/repos/apt/ bionic-pgdg main" > \
    /etc/apt/sources.list.d/pgdg.list && \
  wget --quiet -O - https://www.postgresql.org/media/keys/ACCC4CF8.asc | \
    apt-key add - && \
  apt update ${quiet} && \
  apt install -y -V ${quiet} \
    build-essential \
    ccache \
    debhelper \
    devscripts \
    libgroonga-dev \
    libmsgpack-dev \
    pkg-config \
    postgresql-server-dev-10 && \
  apt clean && \
  rm -rf /var/lib/apt/lists/*
