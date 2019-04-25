#!/bin/bash -xe

TARGET=$1

if [[ -z "$TARGET" ]]; then
    echo "Missing argument: target"
    exit 1
fi

dest="ffrust-deps.zip"
url="https://broth.itch.ovh/ffrust-deps/${TARGET}-head/LATEST/ffrust-deps.zip"
curl --location --fail --output "${dest}" --time-cond "${dest}" "${url}"
rm -rf ffrust-deps
unzip -q -d ffrust-deps ffrust-deps.zip

echo "Go and ffrust in peace"

