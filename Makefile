CC=clang
CFLAGS=-O3
LDFLAGS=-O3

all: libcgmalloc.so
.obj/alloc.o: alloc.h
	$(CC) $(CFLAGS) -c alloc.c -o .obj/alloc.o
.obj/lock.o: lock.h
	$(CC) $(CFLAGS) -c lock.c -o .obj/lock.o
libcgmalloc.so: .obj/alloc.o .obj/lock.o
	$(CC) $(LDFLAGS) -shared -o $@ .obj/*.o
clean:
	rm -f .obj/* bin/*
