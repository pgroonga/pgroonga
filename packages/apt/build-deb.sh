#!/bin/sh

LANG=C

run()
{
  "$@"
  if test $? -ne 0; then
    echo "Failed $@"
    exit 1
  fi
}

. /vagrant/tmp/env.sh

run sudo apt-get update
run sudo apt-get install -y lsb-release apt-transport-https

distribution=$(lsb_release --id --short | tr 'A-Z' 'a-z')
code_name=$(lsb_release --codename --short)
case "${distribution}" in
  debian)
    component=main
    run cat <<EOF | run sudo tee /etc/apt/sources.list.d/groonga.list
deb [signed-by=/usr/share/keyrings/groonga-archive-keyring.gpg] https://packages.groonga.org/debian/ stretch main
deb-src [signed-by=/usr/share/keyrings/groonga-archive-keyring.gpg] https://packages.groonga.org/debian/ stretch main
EOF
    run sudo wget \
        -O /usr/share/keyrings/groonga-archive-keyring.gpg \
        https://packages.groonga.org/debian/groonga-archive-keyring.gpg
    run sudo apt update
    ;;
  ubuntu)
    component=universe
    run sudo apt install -V -y software-properties-common
    run sudo add-apt-repository -y universe
    run sudo add-apt-repository -y ppa:groonga/ppa
    run sudo apt update
    ;;
esac

run sudo apt install -V -y build-essential devscripts ${DEPENDED_PACKAGES}

if [ ${USE_SYSTEM_POSTGRESQL} = "true" ]; then
  run sudo apt install -V -y ${SYSTEM_POSTGRESQL_DEPENDED_PACKAGES}
else
  run cat <<EOF | run sudo tee /etc/apt/sources.list.d/pgdg.list
deb http://apt.postgresql.org/pub/repos/apt/ ${code_name}-pgdg main
EOF
  run wget --quiet -O - https://www.postgresql.org/media/keys/ACCC4CF8.asc | \
    run sudo apt-key add -
  run sudo apt update
  run sudo apt install -V -y ${OFFICIAL_POSTGRESQL_DEPENDED_PACKAGES}
fi

run mkdir -p build
run cp /vagrant/tmp/${PACKAGE}-${VERSION}.tar.gz \
  build/${PACKAGE}_${VERSION}.orig.tar.gz
run cd build
run tar xfz ${PACKAGE}_${VERSION}.orig.tar.gz
run cd ${PACKAGE}-${VERSION}/
run cp -rp /vagrant/tmp/debian debian
# export DEB_BUILD_OPTIONS=noopt
run debuild -us -uc
run cd -

package_initial=$(echo "${PACKAGE}" | sed -e 's/\(.\).*/\1/')
pool_dir="/vagrant/repositories/${distribution}/pool/${code_name}/${component}/${package_initial}/${PACKAGE}"
run mkdir -p "${pool_dir}/"
run cp *.tar.* *.dsc *.deb "${pool_dir}/"
