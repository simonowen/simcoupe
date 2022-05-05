#!/bin/bash
set -e

BUILD_DIR=build-linux
mkdir -p "${BUILD_DIR}" && pushd "${BUILD_DIR}"

cmake .. \
  -DCMAKE_FIND_LIBRARY_SUFFIXES=".a" \
  -DSIMCOUPE_PORTABLE=1

sed -i 's/ SHARED//' _deps/saasound-src/CMakeLists.txt

make -j$(getconf _NPROCESSORS_ONLN)

ldd ./simcoupe
readelf -d ./simcoupe | grep NEEDED

cpack -V -G DEB
DEBFILE=`ls -t _packages/*.deb | head -1`
dpkg --contents "${DEBFILE}" | cut -d' ' -f3-

BASE=$(basename "${DEBFILE}" .deb)
TARFILE="${BASE}.tar.gz"
mkdir -p "${BASE}" "${BASE}-temp"
dpkg --fsys-tarfile "$DEBFILE" | tar -xf - -C "${BASE}-temp"
find "${BASE}-temp" -type f -print0 | xargs -0 -I '{}' mv '{}' "${BASE}"
tar -zvcf "${TARFILE}" "${BASE}"
rm -rf "${BASE}" "${BASE}-temp"

echo DEB: ${BUILD_DIR}/${DEBFILE}
echo TAR: ${BUILD_DIR}/${TARFILE}
