.PHONY: all debug clean modules
CFLAGS=-ldl

all: k-9 modules

debug: CFLAGS += -g -std=c99 -pedantic -Wall -Wextra -D_POSIX_C_SOURCE=200809L
debug: all

modules:
	CFLAGS='$(CFLAGS)' $(MAKE) -C modules # Idfk how to gmake.
