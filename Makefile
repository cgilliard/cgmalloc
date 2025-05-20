CC=clang
CFLAGS=-std=c89 -O3 -pedantic -Wall -Wextra
TEST_CFLAGS=-g

all: alloc.h
	$(CC) $(CFLAGS) -c alloc.c
test: alloc.o test.c
	$(CC) -lcriterion $(TEST_CFLAGS) -o test alloc.o test.c
clean:
	rm -rf *.o test test.dSYM
