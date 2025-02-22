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
  apt install -y -V ${quiet} \
    gnupg \
    lsb-release \
    software-properties-common \
    wget && \
  add-apt-repository -y universe && \
  add-apt-repository -y ppa:groonga/ppa && \
  wget -O /usr/share/keyrings/pgdg.asc https://www.postgresql.org/media/keys/ACCC4CF8.asc && \
  (echo "Types: deb"; \
   echo "URIs: http://apt.postgresql.org/pub/repos/apt"; \
   echo "Suites: $(lsb_release --codename --short)-pgdg"; \
   echo "Components: main"; \
   echo "Signed-By: /usr/share/keyrings/pgdg.asc") | \
    tee /etc/apt/sources.list.d/pgdg.sources && \
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
