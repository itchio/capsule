#!/bin/bash -xe

if [[ -z "$FFMPEG_DIR" ]]; then
    echo "\$FFMPEG_DIR needs to be set to a prefix with x264 & ffmpeg compiled"
    echo "See ./vendor-ffrust-deps.sh and https://fasterthanlime.itch.io/ffrust-deps"
    exit 1
fi

if ! command -v capnpc; then
    echo "capnproto compiler is not in \$PATH"
    echo "Install capnproto 0.7 (binaries on Win32, compile on Linux/macOS)"
    echo "See https://capnproto.org/install.html"
    exit 1
fi

