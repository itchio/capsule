#!/bin/bash -xe
source vendor-env.sh

mkdir -p $CAPSULE_PREFIX/src
pushd $CAPSULE_PREFIX/src

curl --fail --remote-name https://capnproto.org/capnproto-c++-win32-0.7.0.zip
unzip *.zip
pushd capnproto-tools-*

mkdir -p $CAPSULE_PREFIX/bin
cp *.exe $CAPSULE_PREFIX/bin/

popd # capnproto

popd # $CAPSULE_PREFIX/src

