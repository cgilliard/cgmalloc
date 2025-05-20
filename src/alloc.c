/********************************************************************************
 * MIT License
 *
 * Copyright (c) 2025 Christopher Gilliard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 *******************************************************************************/

#include <alloc.h>
#ifndef NO_SPIN_LOCKS
#include <lock.h>
#endif /* NO_SPIN_LOCKS */

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
/* For uint64_t */
#include <stdint.h>

#define HEADER_SIZE 16
#define MAGIC_BYTES 0xAF8BC894322377BCL
#define TRUE 1
#define FALSE 0
#define MAX_SLAB_PTRS 64

#ifndef TEST
#define STATIC static
#else
#define STATIC
#endif

#define SET_BITMAP(chunk, index)                                               \
	do {                                                                   \
		unsigned char *tmp;                                            \
		tmp = (unsigned char *)(chunk);                                \
		tmp[sizeof(ChunkHeader) + (index >> 3)] |= 0x1 << (index & 7); \
	} while (FALSE);

#define UNSET_BITMAP(chunk, index)                         \
	do {                                               \
		unsigned char *tmp;                        \
		tmp = (unsigned char *)(chunk);            \
		tmp[sizeof(ChunkHeader) + (index >> 3)] &= \
		    ~(0x1 << (index & 7));                 \
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

#define BITMAP_INDEX(ptr, chunk)                                       \
	((((size_t)ptr) - (BITMAP_SIZE((chunk)->header.slab_size) +    \
			   sizeof(ChunkHeader) + ((size_t)(chunk)))) / \
	 (chunk)->header.slab_size)

#define SLAB_INDEX(slab_size) \
	(((sizeof(size_t) * 8 - 1) - __builtin_clzl(slab_size)) - 3)

#define NEXT_FREE_BIT(chunk, max, result)                                    \
	do {                                                                 \
		(result) = (size_t) - 1;                                     \
		if ((chunk) != NULL && (max) > 0) {                          \
			uint64_t *bitmap =                                   \
			    (uint64_t *)((unsigned char *)(chunk) +          \
					 sizeof(ChunkHeader));               \
			size_t max_words = ((max) + 63) >> 6;                \
			while (chunk->header.last_free < max_words) {        \
				if (bitmap[chunk->header.last_free] !=       \
				    0xFFFFFFFFFFFFFFFF) {                    \
					uint64_t word =                      \
					    bitmap[chunk->header.last_free]; \
					size_t bit_value =                   \
					    __builtin_ctzll(~word);          \
					size_t index =                       \
					    chunk->header.last_free * 64 +   \
					    bit_value;                       \
					if (index < max) {                   \
						(result) = index;            \
						break;                       \
					}                                    \
				}                                            \
				chunk->header.last_free++;                   \
			}                                                    \
		}                                                            \
	} while (0)

typedef struct Chunk Chunk;

typedef struct {
	uint32_t slab_size;
	uint32_t last_free;
	struct Chunk *next;
	struct Chunk *prev;
	uint64_t magic;
#ifndef NO_SPIN_LOCKS
	Lock lock;
#endif /* NO_SPIN_LOCKS */
} ChunkHeader;

struct Chunk {
	ChunkHeader header;
};

#ifndef NO_SPIN_LOCKS
Lock __alloc_global_lock = LOCK_INIT;
#endif /* NO_SPIN_LOCKS */

Chunk *__alloc_head_ptrs[MAX_SLAB_PTRS] = {0};

STATIC void panic(const char *msg) {
	write(2, msg, strlen(msg));
	exit(-1);
}

/* Return an aligned memory location using specified alignment. */
STATIC void *alloc_aligned_memory(size_t size, size_t alignment) {
	void *base, *aligned_ptr, *suffix_start;
	size_t prefix_size, suffix_size, alloc_size;

	alloc_size = size + alignment;

	/* Call mmap with a large enough allocation so we can align properly. */
	base = mmap(NULL, alloc_size, PROT_READ | PROT_WRITE,
		    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (base == MAP_FAILED) return NULL;

	/* Align the pointer */
	aligned_ptr =
	    (void *)(((size_t)base + alignment - 1) & ~(alignment - 1));

	/* Unmap prefix pages */
	prefix_size = (size_t)aligned_ptr - (size_t)base;
	if (prefix_size) munmap(base, prefix_size);

	/* Unmap suffix pages */
	suffix_size = alloc_size - (prefix_size + size);
	suffix_start = (void *)((size_t)aligned_ptr + size);
	if (suffix_size) munmap(suffix_start, suffix_size);

	return aligned_ptr;
}

/* Calculate slab size based on requested size */
STATIC size_t calculate_slab_size(size_t value) {
	if (value <= 8) return 8;
	return 1UL << (64 - __builtin_clzll(value - 1));
}

STATIC void *alloc_slab(size_t slab_size) {
	Chunk *ptr;
	size_t max = BITMAP_CAPACITY(slab_size);
	size_t index = SLAB_INDEX(slab_size);
	ptr = __alloc_head_ptrs[index];
	if (!ptr) {
#ifndef NO_SPIN_LOCKS
		LockGuard lg = lock_write(&__alloc_global_lock);
#endif /* NO_SPIN_LOCKS */

		/* Check that the pointer is still NULL under lock */
		if (!__alloc_head_ptrs[index]) {
			__alloc_head_ptrs[index] =
			    alloc_aligned_memory(CHUNK_SIZE, CHUNK_SIZE);
			ptr = __alloc_head_ptrs[index];
			if (!ptr) return NULL;
			memset(ptr, 0,
			       sizeof(ChunkHeader) + BITMAP_SIZE(slab_size));
			ptr->header.slab_size = slab_size;
			ptr->header.next = ptr->header.prev = NULL;
			ptr->header.magic = MAGIC_BYTES;
#ifndef NO_SPIN_LOCKS
			ptr->header.lock = LOCK_INIT;
#endif /* NO_SPIN_LOCKS */

			SET_BITMAP(ptr, 0);
			return BITMAP_PTR(ptr, 0, slab_size);
		}
	}

	while (ptr) {
#ifndef NO_SPIN_LOCKS
		LockGuard lg = lock_write(&ptr->header.lock);
#endif /* NO_SPIN_LOCKS */
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
				tmp->header.magic = MAGIC_BYTES;
#ifndef NO_SPIN_LOCKS
				tmp->header.lock = LOCK_INIT;
#endif /* NO_SPIN_LOCKS */
				ptr = tmp;
			}
			continue;
		}
		/* We found a bit set it and return the pointer */
		SET_BITMAP(ptr, bit);
		return BITMAP_PTR(ptr, bit, slab_size);
	}
	return NULL;
}

STATIC void free_slab(void *ptr) {
	Chunk *chunk;
	uint64_t *bitmap64;
	unsigned char *bitmap;
	size_t index, size, chunk_index, i = 0;

	/* Align to chunk header using the property that all Chunks are aligned
	 * to CHUNK_SIZE boundries */
	chunk = (Chunk *)(((size_t)ptr / CHUNK_SIZE) * CHUNK_SIZE);
	if (chunk->header.magic != MAGIC_BYTES)
		panic("Memory corruption: MAGIC not correct. Halting!\n");

	{
#ifndef NO_SPIN_LOCKS
		LockGuard lg = lock_write(&chunk->header.lock);
#endif /* NO_SPIN_LOCKS */

		index = BITMAP_INDEX(ptr, chunk);
		chunk->header.last_free = index / (sizeof(uint64_t) * 8);
		UNSET_BITMAP(chunk, index);

		size = BITMAP_SIZE(chunk->header.slab_size);
		bitmap = (unsigned char *)((size_t)chunk + sizeof(ChunkHeader));
		bitmap64 = (uint64_t *)((unsigned char *)(chunk) +
					sizeof(ChunkHeader));

		if (bitmap64[chunk->header.last_free]) return;
		while (i < size)
			if (bitmap[i++]) return;

		chunk_index = SLAB_INDEX(chunk->header.slab_size);

		{
#ifndef NO_SPIN_LOCKS
			LockGuard globallg = lock_write(&__alloc_global_lock);
#endif /* NO_SPIN_LOCKS */
			if (__alloc_head_ptrs[chunk_index] == chunk)
				__alloc_head_ptrs[chunk_index] =
				    chunk->header.next;
			if (chunk->header.next)
				chunk->header.next->header.prev =
				    chunk->header.prev;
			if (chunk->header.prev)
				chunk->header.prev->header.next =
				    chunk->header.next;
		}
	}

	/* Finally, there are no more bits here, free the chunk */
	munmap(chunk, CHUNK_SIZE);
}

void *cg_malloc(size_t size) {
	if (size > SIZE_MAX - HEADER_SIZE) {
		errno = EINVAL;
		return NULL;
	} else if (size <= MAX_SLAB_SIZE) { /* slab alloc */
		size_t slab_size = calculate_slab_size(size);
		return alloc_slab(slab_size);
	} else { /* large alloc */
		void *ptr;
		size_t aligned_size =
		    (((HEADER_SIZE + size) + PAGE_SIZE - 1) / PAGE_SIZE) *
		    PAGE_SIZE;
		ptr = alloc_aligned_memory(aligned_size, CHUNK_SIZE);
		if (!ptr) /* Could not allocate memory mmap will set errno */
			return NULL;
		else {
			*(uint64_t *)ptr = aligned_size;
			*(uint64_t *)((size_t)ptr + sizeof(uint64_t)) =
			    MAGIC_BYTES;
			return (void *)((size_t)ptr + HEADER_SIZE);
		}
	}
}

void cg_free(void *ptr) {
	void *aligned_ptr = (void *)((size_t)ptr - HEADER_SIZE);
	if (!ptr) return;
	if ((size_t)aligned_ptr % CHUNK_SIZE == 0) { /* large alloc */
		size_t actual_size;
		if (*(uint64_t *)((size_t)aligned_ptr + sizeof(uint64_t)) !=
		    MAGIC_BYTES)
			panic(
			    "Memory corruption: MAGIC not correct. Halting!\n");
		actual_size = *(size_t *)aligned_ptr;
		munmap(aligned_ptr, actual_size);
	} else { /* slab alloc */
		free_slab(ptr);
	}
}

void *cg_calloc(size_t n, size_t size) {
	size_t total_size;
	void *ptr;

	if (n == 0 || size == 0) return NULL;
	if (n > (SIZE_MAX / size)) {
		errno = ENOMEM;
		return NULL;
	}
	total_size = n * size;
	ptr = cg_malloc(total_size);
	if (ptr == NULL) return NULL;
	/* Memset is not needed for larger allocations because we use mmap.
	 * It's only needed for slabs. */
	if (total_size <= MAX_SLAB_SIZE) memset(ptr, 0, total_size);
	return ptr;
}

void *cg_realloc(void *ptr, size_t size) {
	void *new_ptr;
	void *aligned_ptr;
	Chunk *chunk;
	size_t old_size;
	size_t copy_size;
	int is_mmap;

	if (ptr == NULL) {
		return cg_malloc(size);
	}

	if (size == 0) {
		cg_free(ptr);
		return NULL;
	}

	if (size > (size_t)-1 - HEADER_SIZE) {
		errno = ENOMEM;
		return NULL;
	}

	is_mmap = 0;
	old_size = 0;
	aligned_ptr = (void *)((size_t)ptr - HEADER_SIZE);
	if ((size_t)aligned_ptr % CHUNK_SIZE == 0) { /* Large allocation */
		if (*(unsigned long *)((size_t)aligned_ptr + sizeof(size_t)) !=
		    MAGIC_BYTES) {
			errno = EINVAL; /* Invalid magic */
			return NULL;
		}
		old_size = *(size_t *)aligned_ptr;
		is_mmap = 1;
	} else { /* Slab allocation */
		chunk = (Chunk *)(((size_t)ptr / CHUNK_SIZE) * CHUNK_SIZE);
		if (chunk->header.magic != MAGIC_BYTES) {
			errno = EINVAL; /* Invalid magic */
			return NULL;
		}
		old_size = chunk->header.slab_size;
	}

	/* Shrink within same allocator type, reuse ptr */
	if (size <= old_size) {
		if ((size <= MAX_SLAB_SIZE && !is_mmap) ||
		    (size > MAX_SLAB_SIZE && is_mmap)) {
			return ptr;
		}
	}

	/* Allocate new memory */
	new_ptr = cg_malloc(size);
	if (new_ptr == NULL) {
		errno = ENOMEM;
		return NULL; /* Original ptr remains valid */
	}

	/* Copy old data, up to minimum of old and new sizes */
	copy_size = (old_size < size) ? old_size : size;
	memcpy(new_ptr, ptr, copy_size);

	/* Free old memory */
	cg_free(ptr);

	return new_ptr;
}

#ifdef SET_MALLOC
void *malloc(size_t size) { return cg_malloc(size); }
void free(void *ptr) { cg_free(ptr); }
void *calloc(size_t n, size_t size) { return cg_calloc(n, size); }
void *realloc(void *ptr, size_t size) { return cg_realloc(ptr, size); }
void *__wrap_malloc(size_t size) { return cg_malloc(size); }
void __wrap_free(void *ptr) { cg_free(ptr); }
void *__wrap_calloc(size_t n, size_t size) { return cg_calloc(n, size); }
void *__wrap_realloc(void *ptr, size_t size) { return cg_realloc(ptr, size); }
#endif /* SET_MALLOC */
