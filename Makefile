.PHONY: all debug clean

CFLAGS += -DUSE_CURL -lcurl

CFLAGS += -std=c99
CFLAGS += -D_GNU_SOURCE

all: k-9 meow

debug: CFLAGS += -g -O0 -pedantic -Wall -Wextra -D_POSIX_C_SOURCE=200809L -DDEBUG -U_GNU_SOURCE
debug: all

k-9: etc.o
meow: etc.o

clean:
	rm -f k-9 meow etc.o
