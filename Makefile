.PHONY: all debug

all: k-9 meow

debug: CFLAGS += -g -O0 -std=c99 -pedantic -Wall -Wextra -D_POSIX_C_SOURCE=200809L -DDEBUG
debug: all

k-9: printfs.o
meow: printfs.o
