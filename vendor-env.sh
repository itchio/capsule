#!/bin/bash -xe

export CAPSULE_PREFIX=$PWD/vendor_c
export PKG_CONFIG_PATH=$CAPSULE_PREFIX/lib/pkgconfig
# set our bin first, so it uses our version of whatever
# instead of whatever is already installed
export PATH=$CAPSULE_PREFIX/bin:$PATH
export FFMPEG_DIR=$PWD/ffrust-deps
