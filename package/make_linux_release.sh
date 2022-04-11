#!/bin/bash
set -e

BUILD_DIR=build-linux
mkdir -p ${BUILD_DIR} && pushd ${BUILD_DIR}

cmake .. \
  -DCMAKE_FIND_LIBRARY_SUFFIXES=".a" \
  -DSIMCOUPE_PORTABLE=1

sed -i 's/ SHARED//' _deps/saasound-src/CMakeLists.txt

make -j$(getconf _NPROCESSORS_ONLN)

ldd ./simcoupe
readelf -d ./simcoupe | grep NEEDED

cpack -V -G DEB
DEB=`ls -t _packages/*.deb | head -1`
dpkg --contents ${DEB} | cut -d' ' -f3-
echo DEB: ${BUILD_DIR}/${DEB}
