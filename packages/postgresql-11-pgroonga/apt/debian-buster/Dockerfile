ARG FROM=debian:buster
FROM ${FROM}

RUN \
  echo "debconf debconf/frontend select Noninteractive" | \
    debconf-set-selections

ARG DEBUG

RUN \
  quiet=$([ "${DEBUG}" = "yes" ] || echo "-qq") && \
  grep '^deb ' /etc/apt/sources.list | \
    sed -e 's/^deb /deb-src /' > /etc/apt/sources.list.d/base-source.list && \
  apt update ${quiet} && \
  apt install -y -V ${quiet} \
    gnupg \
    wget && \
  { \
    echo "deb [signed-by=/usr/share/keyrings/groonga-archive-keyring.gpg] http://packages.groonga.org/debian/ buster main"; \
    echo "deb-src [signed-by=/usr/share/keyrings/groonga-archive-keyring.gpg] http://packages.groonga.org/debian/ buster main"; \
  } | tee /etc/apt/sources.list.d/groonga.list && \
  wget -O /usr/share/keyrings/groonga-archive-keyring.gpg \
    https://packages.groonga.org/debian/groonga-archive-keyring.gpg && \
  apt update ${quiet} && \
  apt install -y -V ${quiet} \
    build-essential \
    debhelper \
    devscripts \
    libgroonga-dev \
    libmsgpack-dev \
    pkg-config \
    postgresql-server-dev-11
