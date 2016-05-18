
.PHONY: all test main libfake

RUN_CMD := ${GDB} ./main

MAIN_CFLAGS := $(shell pkg-config --cflags glew sdl2)
MAIN_LDFLAGS := $(shell pkg-config --libs glew sdl2)

CC := gcc
CXX := g++
AR := ar

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
	MAIN_LDFLAGS := ${MAIN_LDFLAGS} -lGL
	LIB_CFLAGS := -fPIC
	LIB_LDFLAGS := -shared -ldl
	LIB_EXT := .so
endif

ifeq ($(UNAME_S),Darwin)
	CC := clang
	MAIN_LDFLAGS := ${MAIN_LDFLAGS} -framework OpenGL
	LIB_CFLAGS := -fPIC
	LIB_LDFLAGS := -dynamiclib -ldl -framework Cocoa
	LIB_EXT := .dylib
endif

ifeq (${OS},Windows_NT)
	MAIN_LDFLAGS := ${MAIN_LDFLAGS} -lopengl32 -L . -lfake -g
	LIB_CFLAGS := -DBUILD_LIBFAKE_DLL -I${PWD}/../MologieDetours/
        LIB_LDFLAGS := -shared -ldl -g -L${PWD}/../MologieDetours/build64/ -lMologieDetours
	LIB_EXT := .dll
endif

test: all
ifeq ($(UNAME_S),Linux)
	LD_PRELOAD="${PWD}/libfake.so" MESA_GL_VERSION_OVERRIDE=3.3 MESA_GLSL_VERSION_OVERRIDE=150 ${RUN_CMD}
endif
ifeq ($(UNAME_S),Darwin)
	DYLD_FORCE_FLAT_NAMESPACE=1 DYLD_INSERT_LIBRARIES="${PWD}/libfake.dylib" ${RUN_CMD}
endif
ifeq (${OS},Windows_NT)
	${RUN_CMD}
endif

all: libfake main

main:
	${CXX} ${MAIN_CFLAGS} main.cpp -o main ${MAIN_LDFLAGS}

libfake:
	${CXX} -c ${LIB_CFLAGS} libfake.cpp
ifeq ($(UNAME_S),Darwin)
	${CC} -c ${LIB_CFLAGS} libfake_cocoa.m
endif
ifeq (${OS},Windows_NT)
	${CXX} ${LIB_CFLAGS} libfake*.o -o fake.dll -Wl,--out-implib,libfake.a ${LIB_LDFLAGS}
else
	${AR} rcs libfake.a libfake*.o
	${CXX} ${LIB_CFLAGS} -o libfake${LIB_EXT} libfake*.o ${LIB_LDFLAGS}
	rm -f libfake.a
endif
