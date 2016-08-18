.PHONY: all debug
CFLAGS=-ldl

all: k-9

debug: CFLAGS += -g -std=c99 -pedantic -Wall -Wextra -D_POSIX_C_SOURCE=200809L
debug: all

