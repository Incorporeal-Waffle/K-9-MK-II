.PHONY: all debug clean
CFLAGS=-ldl

all: core

debug: CFLAGS += -g -std=c99 -pedantic -Wall -Wextra -D_POSIX_C_SOURCE=200809L
debug: all
