#!/bin/bash -xe
source vendor-env.sh

mkdir -p $CAPSULE_PREFIX/src
pushd $CAPSULE_PREFIX/src

curl --fail https://capnproto.org/capnproto-c++-0.7.0.tar.gz | tar -xzf -
pushd capnproto-c++-*
./configure --prefix=$CAPSULE_PREFIX
make -j6
make install
popd # capnproto

popd # $CAPSULE_PREFIX/src

