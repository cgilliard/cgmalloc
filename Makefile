CC=clang
CFLAGS=-std=c89 -O3 -pedantic -Wall -Wextra -D_GNU_SOURCE
TEST_CFLAGS=-g
BENCH_FLAGS=-O3 -flto

all: alloc lock
alloc: alloc.h
	$(CC) $(CFLAGS) -c alloc.c
lock: lock.h
	$(CC) $(CFLAGS) -c lock.c
test: all test.c
	$(CC) -lcriterion $(TEST_CFLAGS) -o test alloc.o test.c
bench: all bench.c
	$(CC) $(BENCH_FLAGS) -o bench alloc.o bench.c
clean:
	rm -rf *.o test test.dSYM bench
