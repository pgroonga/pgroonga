ARG FROM=ubuntu:resolute
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
  wget https://packages.apache.org/artifactory/arrow/$(lsb_release --id --short | tr 'A-Z' 'a-z')/apache-arrow-apt-source-latest-$(lsb_release --codename --short).deb && \
  apt install -y -V ${quiet} ./apache-arrow-apt-source-latest-$(lsb_release --codename --short).deb && \
  rm apache-arrow-apt-source-latest-$(lsb_release --codename --short).deb && \
  wget https://packages.groonga.org/ubuntu/groonga-apt-source-latest-$(lsb_release --codename --short).deb && \
  apt install -y -V ./groonga-apt-source-latest-$(lsb_release --codename --short).deb && \
  rm groonga-apt-source-latest-$(lsb_release --codename --short).deb && \
  apt install -y postgresql-common && \
  /usr/share/postgresql-common/pgdg/apt.postgresql.org.sh -y && \
  apt install -y -V ${quiet} \
    build-essential \
    ccache \
    debhelper \
    devscripts \
    libgroonga-dev \
    libmsgpack-dev \
    libxxhash-dev \
    meson \
    ninja-build \
    pkg-config \
    postgresql-server-dev-${POSTGRESQL_VERSION} && \
  apt clean && \
  rm -rf /var/lib/apt/lists/*

RUN { \
      echo "DEBUILD_LINTIAN=no"; \
    } > ~/.devscripts
