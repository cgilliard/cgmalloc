#include "cgmalloc.h"

/* For mmap/munmap */
#include <sys/mman.h>
/* For write, getpagesize */
#include <unistd.h>
/* For exit */
#include <stdlib.h>
/* For error handling */
#include <errno.h>
/* For memset, strlen */
#include <string.h>

#define HEADER_SIZE 16
#if __SIZEOF_POINTER__ == 4
#define MAGIC_BYTES 0xABCD123A
#else
#define MAGIC_BYTES 0xAF8BC894322377BC
#endif
#define TRUE 1
#define FALSE 0
#define MAX_SLAB_PTRS 32

#include <stdio.h>

static void panic(const char *msg) {
	write(2, msg, strlen(msg));
	exit(-1);
}

static void *alloc_aligned_memory(size_t size, size_t alignment) {
	void *base, *aligned_ptr, *suffix_start;
	size_t prefix_size, actual_size, suffix_size;

	base = mmap(NULL, size * 2, PROT_READ | PROT_WRITE,
		    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (base == MAP_FAILED) return NULL;

	aligned_ptr =
	    (void *)(((size_t)base + alignment - 1) & ~(alignment - 1));
	prefix_size = (size_t)aligned_ptr - (size_t)base;
	if (prefix_size) munmap(base, prefix_size);

	suffix_size = ((size * 2) - prefix_size) - size;
	suffix_start = (void *)((size_t)aligned_ptr + size);
	if (suffix_size) munmap(suffix_start, suffix_size);

	actual_size = (size * 2) - prefix_size;
	*(size_t *)aligned_ptr = actual_size;
	*(size_t *)((size_t)aligned_ptr + sizeof(size_t)) = MAGIC_BYTES;

	return aligned_ptr;
}

static size_t calculate_slab_size(size_t value) {
	if (value <= 8) return 8;
	value--;
	value |= value >> 1;
	value |= value >> 2;
	value |= value >> 4;
	value |= value >> 8;
	value |= value >> 16;
	if (sizeof(size_t) > 4) value |= value >> 32;
	return value + 1;
}

typedef struct Chunk Chunk;

typedef struct {
	uint32_t slab_size;
	struct Chunk *next;
	struct Chunk *prev;
} ChunkHeader;

struct Chunk {
	ChunkHeader header;
};

Chunk *__cg_malloc_head_ptrs[MAX_SLAB_PTRS] = {0};

#define SET_BITMAP(chunk, index)                                               \
	do {                                                                   \
		unsigned char *tmp;                                            \
		tmp = (unsigned char *)(chunk);                                \
		tmp[sizeof(ChunkHeader) + (index >> 3)] |= 0x1 << (index & 7); \
	} while (FALSE);

#define BITMAP_SIZE(slab_size)                              \
	((((8 * CHUNK_SIZE - 8 * sizeof(ChunkHeader) - 7) / \
	   (1 + 8 * (slab_size))) +                         \
	  7) >>                                             \
	 3)

#define BITMAP_CAPACITY(slab_size) \
	((8 * CHUNK_SIZE - 8 * sizeof(ChunkHeader) - 7) / (1 + 8 * (slab_size)))

#define BITMAP_PTR(chunk, index, slab_size)             \
	((unsigned char *)chunk + sizeof(ChunkHeader) + \
	 BITMAP_SIZE(slab_size) + (index * slab_size))

#define NEXT_FREE_BIT(chunk, max, result)                                     \
	do {                                                                  \
		(result) = (size_t) - 1;                                      \
		if ((chunk) != NULL && (max) > 0) {                           \
			unsigned char *__bitmap =                             \
			    (unsigned char *)(chunk) + sizeof(ChunkHeader);   \
			size_t __max_bytes =                                  \
			    ((max) + 7) >> 3; /* Ceiling of max/8 */          \
			size_t __byte;                                        \
			for (__byte = 0;                                      \
			     __byte < __max_bytes && __byte * 8 < (max);      \
			     __byte++) {                                      \
				if (__bitmap[__byte] != 0xFF) {               \
					unsigned char __b = __bitmap[__byte]; \
					size_t __bit;                         \
					for (__bit = 0;                       \
					     __bit < 8 &&                     \
					     __byte * 8 + __bit < (max);      \
					     __bit++) {                       \
						if (!(__b & (1U << __bit))) { \
							(result) =            \
							    __byte * 8 +      \
							    __bit;            \
							break;                \
						}                             \
					}                                     \
					if ((result) != (size_t) - 1) break;  \
				}                                             \
			}                                                     \
		}                                                             \
	} while (FALSE)

static void *alloc_slab(size_t slab_size) {
	Chunk *ptr;
	size_t max = BITMAP_CAPACITY(slab_size);
	size_t index =
	    ((sizeof(size_t) * 8 - 1) - __builtin_clzl(slab_size)) - 3;
	if (index >= MAX_SLAB_PTRS || max == 0) {
		errno = EINVAL;
		return NULL;
	}
	ptr = __cg_malloc_head_ptrs[index];
	if (!ptr) {
		ptr = alloc_aligned_memory(CHUNK_SIZE, CHUNK_SIZE);
		if (!__cg_malloc_head_ptrs[index]) return NULL;
		memset(ptr, 0, sizeof(ChunkHeader) + BITMAP_SIZE(slab_size));
		ptr->header.slab_size = slab_size;
		ptr->header.next = ptr->header.prev = NULL;
		SET_BITMAP(ptr, 0);
		return BITMAP_PTR(ptr, 0, slab_size);
	}

	while (ptr) {
		size_t bit;
		NEXT_FREE_BIT(ptr, max, bit);
		if (bit == (size_t)-1) {
			Chunk *tmp;
			tmp = ptr->header.next;
			if (tmp)
				ptr = tmp;
			else {
				tmp = alloc_aligned_memory(CHUNK_SIZE,
							   CHUNK_SIZE);
				if (!tmp) break;
				memset(tmp, 0,
				       sizeof(ChunkHeader) +
					   BITMAP_SIZE(slab_size));
				ptr->header.next = tmp;
				tmp->header.prev = ptr;
				tmp->header.next = NULL;
				tmp->header.slab_size = slab_size;
				ptr = tmp;
			}
			continue;
		}
		SET_BITMAP(ptr, bit);
		return BITMAP_PTR(ptr, bit, slab_size);
	}
	return NULL;
}

void *cgmalloc(size_t size) {
	if (size < MAX_SLAB_SIZE) {
		size_t slab_size = calculate_slab_size(size);
		return alloc_slab(slab_size);
	} else {
		void *ptr;
		ptr = alloc_aligned_memory(size, CHUNK_SIZE);
		return (void *)((size_t)ptr + HEADER_SIZE);
	}
}

void cgfree(void *ptr) {
	void *aligned_ptr;

	if (!ptr) return;

	aligned_ptr = (void *)((size_t)ptr - HEADER_SIZE);
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
