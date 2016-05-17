
.PHONY: all test main libfake

MAIN_CFLAGS := $(shell sdl2-config --cflags) $(shell pkg-config --cflags glew)
MAIN_LDFLAGS := $(shell sdl2-config --libs) $(shell pkg-config --libs glew)

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
	LIB_CFLAGS := -shared -fPIC -D_CAPSULE_LINUX
	LIB_EXT := .so
endif
ifeq ($(UNAME_S),Darwin)
	LIB_CFLAGS := -dynamiclib -D_CAPSULE_OSX
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
