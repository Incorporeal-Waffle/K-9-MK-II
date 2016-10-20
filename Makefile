.PHONY: all debug clean

CFLAGS += -D_GNU_SOURCE
CFLAGS += -DUSE_CURL -lcurl

all: k-9 meow

debug: CFLAGS += -g -O0 -std=c99 -pedantic -Wall -Wextra -D_POSIX_C_SOURCE=200809L -DDEBUG -U_GNU_SOURCE
debug: all

k-9: etc.o
meow: etc.o

clean:
	rm -f k-9 meow etc.o
