ARG FROM=debian:stretch
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
    echo "deb [signed-by=/usr/share/keyrings/groonga-archive-keyring.gpg] http://packages.groonga.org/debian/ stretch main"; \
    echo "deb-src [signed-by=/usr/share/keyrings/groonga-archive-keyring.gpg] http://packages.groonga.org/debian/ stretch main"; \
  } | tee /etc/apt/sources.list.d/groonga.list && \
  wget -O /usr/share/keyrings/groonga-archive-keyring.gpg \
    https://packages.groonga.org/debian/groonga-archive-keyring.gpg && \
  echo "deb http://apt.postgresql.org/pub/repos/apt/ stretch-pgdg main" > \
    /etc/apt/sources.list.d/pgdg.list && \
  wget --quiet -O - https://www.postgresql.org/media/keys/ACCC4CF8.asc | \
    apt-key add - && \
  apt update ${quiet} && \
  apt install -y -V ${quiet} \
    build-essential \
    debhelper \
    devscripts \
    libgroonga-dev \
    libmsgpack-dev \
    pkg-config \
    postgresql-server-dev-11
