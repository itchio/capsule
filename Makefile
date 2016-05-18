
.PHONY: all test main libfake

RUN_CMD := ${GDB} ./main

MAIN_CFLAGS := $(shell pkg-config --cflags glew sdl2)
MAIN_LDFLAGS := $(shell pkg-config --libs glew sdl2)

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
	MAIN_LDFLAGS := ${MAIN_LDFLAGS} -lGL
	LIB_CFLAGS := -fPIC
	LIB_LDFLAGS := -shared -ldl
	LIB_EXT := .so
endif

ifeq ($(UNAME_S),Darwin)
	MAIN_LDFLAGS := ${MAIN_LDFLAGS} -framework OpenGL
	LIB_CFLAGS := -fPIC
	LIB_LDFLAGS := -dynamiclib -ldl -framework Cocoa
	LIB_EXT := .dylib
endif

ifeq (${OS},Windows_NT)
	MAIN_LDFLAGS := ${MAIN_LDFLAGS} -lopengl32
endif

CC ?= clang
AR := ar

test: all
	mkdir -p frames
ifeq ($(UNAME_S),Linux)
	LD_PRELOAD="${PWD}/libfake.so" MESA_GL_VERSION_OVERRIDE=3.3 MESA_GLSL_VERSION_OVERRIDE=150 ${RUN_CMD}
endif
ifeq ($(UNAME_S),Darwin)
	DYLD_FORCE_FLAT_NAMESPACE=1 DYLD_INSERT_LIBRARIES="${PWD}/libfake.dylib" ${RUN_CMD}
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
