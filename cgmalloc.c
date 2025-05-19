#include "cgmalloc.h"

/* For mmap/munmap */
#include <sys/mman.h>
/* For sysconf(PAGE_SIZE) */
#include <unistd.h>
/* For exit */
#include <stdlib.h>

#define PAGE_SIZE (getpagesize())
#define MAGIC_BYTES 0xAF8BC894322377BC

static size_t strlen(const char *X) {
	const char *Y;
	if (X == NULL) return 0;
	Y = X;
	while (*X) X++;
	return X - Y;
}

static void panic(const char *msg) {
	write(2, msg, strlen(msg));
	exit(-1);
}

static void *allocate_aligned_memory(size_t size, size_t alignment) {
	void *base, *aligned_ptr;
	size_t prefix_size, actual_size;

	base = mmap(NULL, size * 2, PROT_READ | PROT_WRITE,
		    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (base == MAP_FAILED) {
		return NULL;
	}

	aligned_ptr =
	    (void *)(((size_t)base + alignment - 1) & ~(alignment - 1));
	prefix_size = (size_t)aligned_ptr - (size_t)base;
	munmap(base, prefix_size);
	actual_size = (size * 2) - prefix_size;
	*(size_t *)aligned_ptr = actual_size;
	*(size_t *)((size_t)aligned_ptr + sizeof(size_t)) = MAGIC_BYTES;
	return aligned_ptr;
}

void *cgmalloc(size_t size) {
	size_t alloc_size;
	void *ptr;

	/* TODO: for now we just allocate large blocks, but later implement */
	/* slabs for < CHUNK_SIZE */
	if (size < CHUNK_SIZE)
		alloc_size = CHUNK_SIZE;
	else
		alloc_size = size;
	ptr = allocate_aligned_memory(alloc_size, CHUNK_SIZE);

	return (void *)((size_t)ptr + 16);
}
void cgfree(void *ptr) {
	void *aligned_ptr;
	aligned_ptr = (void *)((size_t)ptr - 16);
	if ((size_t)aligned_ptr % CHUNK_SIZE == 0) {
		size_t actual_size;
		if (*(size_t *)((size_t)aligned_ptr + sizeof(size_t)) !=
		    MAGIC_BYTES)
			panic(
			    "Memory corruption: MAGIC not correct. Halting!\n");
		actual_size = *(size_t *)aligned_ptr;
		munmap(aligned_ptr, actual_size);
	} else {
		/* TODO: handle slab frees */
	}
}

/*
void *cgcalloc(size_t n, size_t size) { return NULL; }
void *cgrealloc(void *ptr, size_t size) { return NULL; }
*/
