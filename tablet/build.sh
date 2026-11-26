#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
: "${SDK_ENV:?Set SDK_ENV to the official Paper Pro (ferrari) SDK environment-setup file}"
: "${XOVI_REPO:?Set XOVI_REPO to the xovi source checkout}"
set +u
source "$SDK_ENV"
set -u
case "$($CC -dumpmachine)" in aarch64*) ;; *) echo "Expected ARM64 Paper Pro SDK"; exit 1 ;; esac
python3 "$XOVI_REPO/util/xovigen.py" -o xovi.cpp -H xovi.h reply.xovi
test -s xovi.cpp && test -s xovi.h
mkdir -p build
cd build
qmake ../reply.pro CONFIG+=release
make -j"${JOBS:-4}"
mkdir -p ../bin
cp reply.so ../bin/reply.so
$STRIP --strip-unneeded ../bin/reply.so

$CXX -std=c++17 -fPIC ../src/config-model.cpp -o ../bin/config-model $(pkg-config --cflags --libs Qt6Core)
$STRIP --strip-unneeded ../bin/config-model
