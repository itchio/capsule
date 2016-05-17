
.PHONY: all test main libfake

MAIN_CFLAGS := $(shell sdl2-config --cflags) $(shell pkg-config --cflags glew)
MAIN_LDFLAGS := $(shell sdl2-config --libs) $(shell pkg-config --libs glew)

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
  	MAIN_CFLAGS := ${MAIN_CFLAGS} -lGL
	LIB_CFLAGS := -shared -fPIC -D_CAPSULE_LINUX
	LIB_EXT := .so
endif
ifeq ($(UNAME_S),Darwin)
  	MAIN_CFLAGS := ${MAIN_CFLAGS} -framework OpenGL
	LIB_CFLAGS := -D_CAPSULE_OSX
	LIB_LDFLAGS := -dynamiclib
	LIB_EXT := .dylib
endif
CC := clang
AR := ar

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
	${CC} -c ${LIB_CFLAGS} libfake.c
ifeq ($(UNAME_S),Darwin)
	${CC} -c ${LIB_CFLAGS} libfake_cocoa.m
endif
	${AR} rcs libfake.a libfake*.o
	${CC} ${LIB_CFLAGS} -ldl libfake*.o -o libfake${LIB_EXT} ${LIB_LDFLAGS}
