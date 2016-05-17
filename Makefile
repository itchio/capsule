
.PHONY: all test main libfake

MAIN_CFLAGS := $(shell sdl2-config --cflags)
MAIN_LDFLAGS := $(shell sdl2-config --libs)

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
	LIB_CFLAGS := -shared -fPIC
	LIB_EXT := .so
endif
ifeq ($(UNAME_S),Darwin)
	LIB_CFLAGS := -dynamiclib
	LIB_EXT := .dylib
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
	${CC} ${MAIN_CFLAGS} main.c -o main ${MAIN_LDFLAGS}

libfake:
	${CC} ${LIB_CFLAGS} -ldl libfake.c -o libfake${LIB_EXT}
