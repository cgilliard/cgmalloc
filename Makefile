CC=clang
CFLAGS=-O3
LDFLAGS=-O3
ALLOCFLAGS=
BENCHFLAGS=-O3 -flto

all: lib/libcgmalloc.so
.obj/alloc.o: include/alloc.h src/alloc.c
	$(CC) -Iinclude $(CFLAGS) $(ALLOCFLAGS) -c src/alloc.c -o .obj/alloc.o
.obj/lock.o: include/lock.h src/lock.c
	$(CC) -Iinclude $(CFLAGS) -c src/lock.c -o .obj/lock.o
lib/libcgmalloc.so: .obj/alloc.o .obj/lock.o
	$(CC) $(LDFLAGS) -shared -o $@ .obj/*.o
test: src/test.c .obj/lock.o .obj/alloc.o
	$(CC) -Iinclude -lcriterion -g -o bin/test .obj/*.o src/test.c
bench: src/bench.c .obj/lock.o .obj/alloc.o
	$(CC) -Iinclude $(BENCHFLAGS) -o bin/bench .obj/*.o src/bench.c
clean:
	rm -f .obj/* lib/*
