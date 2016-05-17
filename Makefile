
.PHONY: all main libfake

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
	LD_FLAGS := -shared
	LD_EXT := .so
endif
ifeq ($(UNAME_S),Darwin)
	LD_FLAGS := -dynamiclib
	LD_EXT := .dylib
endif
CC := clang

all: main libfake

main:
	${CC} -lm main.c -o main

libfake:
	${CC} -c libfake.c -o libfake.o
	${CC} ${LD_FLAGS} libfake.o -o libfake${LD_EXT}
