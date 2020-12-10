FROM fedora:33

ARG DEBUG

RUN \
  quiet=$([ "${DEBUG}" = "yes" ] || echo "--quiet") && \
  dnf install -y ${quiet} \
    fedora-packager \
    rpmdevtools && \
  dnf install -y ${quiet} \
    ccache \
    clang \
    gcc \
    groonga-devel \
    libpq-devel \
    llvm-devel \
    make \
    msgpack-devel \
    postgresql-server-devel \
    xxhash-devel && \
  dnf clean -y ${quiet} all

# TODO: Remove this: 17 == $(( 0x0001|0x0010 ))
ENV QA_RPATHS=17
