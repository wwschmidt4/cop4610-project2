CC = gcc
CFLAGS = -Wall -Wextra

.PHONY: all run clean

all: empty part1

empty: empty.c
	$(CC) $(CFLAGS) -o $@ $<

part1: part1.c
	$(CC) $(CFLAGS) -o $@ $<

run: empty part1
	strace -o empty.trace ./empty
	strace -o part1.trace ./part1

clean:
	rm -f empty part1