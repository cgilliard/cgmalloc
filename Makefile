CC=clang
CFLAGS=-std=c89 -O3 -pedantic -Wall -Wextra
TEST_CFLAGS=-g
BENCH_FLAGS=-O3 -flto

all: alloc.h
	$(CC) $(CFLAGS) -c alloc.c
test: alloc.o test.c
	$(CC) -lcriterion $(TEST_CFLAGS) -o test alloc.o test.c
bench: alloc.o bench.c
	$(CC) $(BENCH_FLAGS) -o bench alloc.o bench.c
clean:
	rm -rf *.o test test.dSYM bench
