CC=clang
CFLAGS=-O3

alloc: alloc.h
	$(CC) $(CFLAGS) -c alloc.c
