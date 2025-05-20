CC=clang
CFLAGS=-O3
LDFLAGS=-O3
ALLOCFLAGS=
BENCHFLAGS=-O3 -flto

all: lib/libcgmalloc.so
.obj/alloc.o: alloc.h alloc.c
	$(CC) $(CFLAGS) $(ALLOCFLAGS) -c alloc.c -o .obj/alloc.o
.obj/lock.o: lock.h lock.c
	$(CC) $(CFLAGS) -c lock.c -o .obj/lock.o
lib/libcgmalloc.so: .obj/alloc.o .obj/lock.o
	$(CC) $(LDFLAGS) -shared -o $@ .obj/*.o
test: test.c .obj/lock.o .obj/alloc.o
	$(CC) -lcriterion -g -o bin/test .obj/*.o test.c
bench: bench.c .obj/lock.o .obj/alloc.o
	$(CC) $(BENCHFLAGS) -o bin/bench .obj/*.o bench.c
clean:
	rm -f .obj/* lib/*
