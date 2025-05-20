CC = clang
CFLAGS = -fPIC -std=c89 -pedantic -Wall -Wextra -O3 -D_GNU_SOURCE
LDFLAGS = -O3
ALLOCFLAGS =
BENCHFLAGS = -O3 -flto
TESTFLAGS = -DTEST -g

# Directories
OBJDIR = .obj
LIBDIR = lib
BINDIR = bin

# Default target
all: $(LIBDIR)/libcgmalloc.so

# Object files for library and bench
$(OBJDIR)/alloc.o: include/alloc.h src/alloc.c
	$(CC) -Iinclude $(CFLAGS) $(ALLOCFLAGS) -c src/alloc.c -o $@

$(OBJDIR)/lock.o: include/lock.h src/lock.c
	$(CC) -Iinclude $(CFLAGS) -c src/lock.c -o $@

# Object files for test (with TESTFLAGS)
$(OBJDIR)/alloc_test.o: include/alloc.h src/alloc.c
	$(CC) -Iinclude $(ALLOCFLAGS) $(TESTFLAGS) -c src/alloc.c -o $@

$(OBJDIR)/lock_test.o: include/lock.h src/lock.c
	$(CC) -Iinclude $(TESTFLAGS) -c src/lock.c -o $@

# Shared library
$(LIBDIR)/libcgmalloc.so: $(OBJDIR)/alloc.o $(OBJDIR)/lock.o
	$(CC) $(LDFLAGS) -shared -o $@ $(OBJDIR)/alloc.o $(OBJDIR)/lock.o

# Test binary (uses test-specific objects)
test: src/test.c $(OBJDIR)/lock_test.o $(OBJDIR)/alloc_test.o
	$(CC) -Iinclude -lcriterion -g -o $(BINDIR)/test $(OBJDIR)/alloc_test.o $(OBJDIR)/lock_test.o src/test.c

# Benchmark binary
bench: src/bench.c $(OBJDIR)/lock.o $(OBJDIR)/alloc.o
	$(CC) -Iinclude $(BENCHFLAGS) -o $(BINDIR)/bench $(OBJDIR)/alloc.o $(OBJDIR)/lock.o src/bench.c

# Clean up
clean:
	rm -fr $(OBJDIR)/* $(LIBDIR)/* $(BINDIR)/*

# Phony targets
.PHONY: all test bench clean
