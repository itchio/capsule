
.PHONY: all test main libfake

RUN_CMD := ${GDB} ./main

MAIN_CFLAGS := $(shell sdl2-config --cflags) $(shell pkg-config --cflags glew)
MAIN_LDFLAGS := $(shell sdl2-config --libs) $(shell pkg-config --libs glew)

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
  	MAIN_CFLAGS := ${MAIN_CFLAGS} -lGL
	LIB_CFLAGS := -fPIC -D_CAPSULE_LINUX
	LIB_LDFLAGS := -shared -ldl
	LIB_EXT := .so
endif
ifeq ($(UNAME_S),Darwin)
  	MAIN_CFLAGS := ${MAIN_CFLAGS} -framework OpenGL
	LIB_CFLAGS := -fPIC -D_CAPSULE_OSX
	LIB_LDFLAGS := -dynamiclib -ldl -framework Cocoa
	LIB_EXT := .dylib
endif
CC := clang
AR := ar

test: all
	mkdir -p frames
ifeq ($(UNAME_S),Linux)
	LD_PRELOAD="${PWD}/libfake.so" MESA_GL_VERSION_OVERRIDE=3.3 MESA_GLSL_VERSION_OVERRIDE=150 ${RUN_CMD}
endif
ifeq ($(UNAME_S),Darwin)
	DYLD_FORCE_FLAT_NAMESPACE=1 DYLD_INSERT_LIBRARIES="${PWD}/libfake.dylib" DYLD_LIBRARY_PATH="${PWD}" ${RUN_CMD}
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
	${CC} ${LIB_CFLAGS} libfake*.o -o libfake${LIB_EXT} ${LIB_LDFLAGS}
