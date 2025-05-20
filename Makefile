CC = clang
CFLAGS = -std=c89 -pedantic -Wall -Wextra -O3
TEST_CFLAGS = -g
BENCH_FLAGS = -O3 -flto
LDFLAGS = -lcriterion

SRCS = alloc.c lock.c test.c bench.c
OBJS = alloc.o lock.o test.o bench.o
DEPS = $(SRCS:.c=.d)

all: test bench

alloc.o: alloc.c alloc.h
	$(CC) $(CFLAGS) -c $< -o $@

lock.o: lock.c lock.h
	$(CC) $(CFLAGS) -c $< -o $@

test.o: test.c
	$(CC) $(TEST_CFLAGS) -c $< -o $@

bench.o: bench.c
	$(CC) $(BENCH_FLAGS) -c $< -o $@

test: alloc.o lock.o test.o
	$(CC) $(TEST_CFLAGS) -o $@ $^ $(LDFLAGS)

bench: alloc.o lock.o bench.o
	$(CC) $(BENCH_FLAGS) -o $@ $^

%.d: %.c
	$(CC) $(CFLAGS) -M $< > $@

-include $(DEPS)

clean:
	rm -rf *.o *.d test test.dSYM bench

.PHONY: all clean
