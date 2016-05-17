
.PHONY: all test main libfake

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
	LD_FLAGS := -shared -fPIC
	LD_EXT := .so
endif
ifeq ($(UNAME_S),Darwin)
	LD_FLAGS := -dynamiclib
	LD_EXT := .dylib
endif
CC := clang

test: all
ifeq ($(UNAME_S),Linux)
	LD_PRELOAD="${PWD}/libfake.so" ./main
endif
ifeq ($(UNAME_S),Darwin)
	DYLD_FORCE_FLAT_NAMESPACE=1 DYLD_INSERT_LIBRARIES="${PWD}/libfake.dylib" ./main
endif

all: main libfake

main:
	${CC} -lm main.c -o main

libfake:
	${CC} ${LD_FLAGS} -ldl libfake.c -o libfake${LD_EXT}
