CC = clang
CFLAGS = -fPIC -std=c89 -pedantic -Wall -Wextra -O3 -D_GNU_SOURCE
TEST_CFLAGS = -g
BENCH_FLAGS = -O3 -flto
TEST_LDFLAGS = -lcriterion
LDFLAGS = -O3 -flto
SET_MALLOC =

SRCS = alloc.c lock.c test.c bench.c
OBJS = alloc.o lock.o test.o bench.o
DEPS = $(SRCS:.c=.d)

all: test bench lib

alloc.o: alloc.c alloc.h
	$(CC) $(CFLAGS) $(SET_MALLOC) -c $< -o $@

lock.o: lock.c lock.h
	$(CC) $(CFLAGS) -c $< -o $@

test.o: test.c
	$(CC) $(TEST_CFLAGS) -c $< -o $@

bench.o: bench.c
	$(CC) $(BENCH_FLAGS) -c $< -o $@

lib: libcgmalloc.so

libcgmalloc.so: alloc.o lock.o
	$(CC) $(LDFLAGS) -shared -o $@ $^

test: alloc.o lock.o test.o
	$(CC) $(TEST_CFLAGS) -o $@ $^ $(TEST_LDFLAGS)

bench: alloc.o lock.o bench.o
	$(CC) $(BENCH_FLAGS) -o $@ $^

install:
	sudo cp libcgmalloc.so /usr/local/lib/

# Prevent automatic dependency generation from triggering builds
%.d: %.c
	@$(CC) $(CFLAGS) -M $< > $@

-include $(DEPS)

clean:
	rm -rf *.o *.so *.d test test.dSYM bench

.PHONY: all clean lib test bench install
