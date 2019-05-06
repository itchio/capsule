#!/bin/bash -e

if [[ -z "$FFMPEG_DIR" ]]; then
    echo "Error: \$FFMPEG_DIR is not set."
    echo "You need to source ./vendor-env.sh to build capsule"
    exit 1
fi

if [[ ! -d "$FFMPEG_DIR" ]]; then
    echo "ffrust deps missing, trying to install..."
    triplet=$(rustup show active-toolchain | cut -d ' ' -f 1)
    triplet=$(echo ${triplet} | sed 's/stable-//g')
    ./vendor-ffrust.sh ${triplet}
fi

if ! command -v capnpc; then
    echo "Error: capnproto compiler is not in \$PATH"
    echo "Install capnproto 0.7 (binaries on Win32, compile on Linux/macOS)"
    echo "See https://capnproto.org/install.html"
    exit 1
fi

