CARGO_FLAGS:=${CARGO_FLAGS}

all:
	echo "No defaults targets, use 'make linux', 'make windows', etc."

macos: macos64

macos64:
	rustup run "stable-x86_64-apple-darwin" cargo build ${CARGO_FLAGS} --target "x86_64-apple-darwin"

linux: linux64 linux32

linux64:
	rustup run "stable-x86_64-unknown-linux-gnu" cargo build ${CARGO_FLAGS} --target "x86_64-unknown-linux-gnu"

linux32:
	rustup run "stable-i686-unknown-linux-gnu" cargo build ${CARGO_FLAGS} --target "i686-unknown-linux-gnu"

