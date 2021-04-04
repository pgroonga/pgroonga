ARG FROM=debian:buster
FROM ${FROM}

RUN \
  echo "debconf debconf/frontend select Noninteractive" | \
    debconf-set-selections

ARG DEBUG

RUN \
  quiet=$([ "${DEBUG}" = "yes" ] || echo "-qq") && \
  apt update ${quiet} && \
  apt install -y -V ${quiet} wget && \
  wget https://packages.groonga.org/debian/groonga-apt-source-latest-buster.deb && \
  apt install -y -V ./groonga-apt-source-latest-buster.deb && \
  rm groonga-apt-source-latest-buster.deb && \
  echo "deb http://apt.postgresql.org/pub/repos/apt/ buster-pgdg main" > \
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
    postgresql-server-dev-13 && \
  apt clean && \
  rm -rf /var/lib/apt/lists/*
