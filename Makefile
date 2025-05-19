CC=clang
CFLAGS=-std=c89 -O3 -pedantic -Wall -Wextra
TEST_CFLAGS=-g

all: cgmalloc.h
	$(CC) $(CFLAGS) -c cgmalloc.c
test: cgmalloc.o test.c
	$(CC) -lcriterion $(TEST_CFLAGS) -o test cgmalloc.o test.c
clean:
	rm -rf *.o test test.dSYM
