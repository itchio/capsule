CARGO_FLAGS:=${CARGO_FLAGS}

.PHONY: all check

all:
	echo "No defaults targets, use 'make linux', 'make windows', etc."

check:
	./release/check.sh

macos: macos64

macos64: check
	rustup run "stable-x86_64-apple-darwin" cargo build ${CARGO_FLAGS} --target "x86_64-apple-darwin"

linux: linux64 linux32

linux64: check
	rustup run "stable-x86_64-unknown-linux-gnu" cargo build ${CARGO_FLAGS} --target "x86_64-unknown-linux-gnu"

linux32: check
	rustup run "stable-i686-unknown-linux-gnu" cargo build ${CARGO_FLAGS} --target "i686-unknown-linux-gnu"

windows: windows64 windows32

windows64: check
	rustup run "stable-x86_64-pc-windows-msvc" cargo build ${CARGO_FLAGS} --target "x86_64-pc-windows-msvc"

windows32: check
	rustup run "stable-i686-pc-windows-msvc" cargo build ${CARGO_FLAGS} --target "i686-pc-windows-msvc"
