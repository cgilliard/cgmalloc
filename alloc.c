#include "alloc.h"

#include "lock.h"

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
#define MAX_SLAB_PTRS 32

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
			size_t i;                                            \
			for (i = chunk->header.last_free; i < max_words;     \
			     i++) {                                          \
				if (bitmap[i] != 0xFFFFFFFFFFFFFFFF) {       \
					uint64_t word = bitmap[i];           \
					size_t bit_value =                   \
					    __builtin_ctzll(~word);          \
					size_t index = i * 64 + bit_value;   \
					if (index < max) {                   \
						chunk->header.last_free = i; \
						(result) = index;            \
						break;                       \
					}                                    \
				}                                            \
			}                                                    \
		}                                                            \
	} while (0)

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

	suffix_size = size - prefix_size;
	suffix_start = (void *)((size_t)aligned_ptr + size);
	if (suffix_size) munmap(suffix_start, suffix_size);

	actual_size = (size * 2) - prefix_size;
	*(uint64_t *)aligned_ptr = actual_size;
	*(uint64_t *)((size_t)aligned_ptr + sizeof(uint64_t)) = MAGIC_BYTES;
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
	uint32_t last_free;
	struct Chunk *next;
	struct Chunk *prev;
	uint64_t magic;
	Lock lock;
} ChunkHeader;

struct Chunk {
	ChunkHeader header;
};

Lock __alloc_global_lock = LOCK_INIT;
Chunk *__alloc_head_ptrs[MAX_SLAB_PTRS] = {0};

static void *alloc_slab(size_t slab_size) {
	Chunk *ptr;
	size_t max = BITMAP_CAPACITY(slab_size);
	size_t index = SLAB_INDEX(slab_size);
	if (index >= MAX_SLAB_PTRS || max == 0) {
		errno = EINVAL;
		return NULL;
	}
	ptr = __alloc_head_ptrs[index];
	if (!ptr) {
		LockGuard lg __attribute__((unused)) =
		    lock_write(&__alloc_global_lock);

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
			ptr->header.lock = LOCK_INIT;
			SET_BITMAP(ptr, 0);
			return BITMAP_PTR(ptr, 0, slab_size);
		}
	}

	while (ptr) {
		LockGuard lg __attribute__((unused)) =
		    lock_write(&ptr->header.lock);
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
				tmp->header.lock = LOCK_INIT;
				ptr = tmp;
			}
			continue;
		}
		SET_BITMAP(ptr, bit);
		return BITMAP_PTR(ptr, bit, slab_size);
	}
	return NULL;
}

static void free_slab(void *ptr) {
	Chunk *chunk;
	uint64_t *bitmap64;
	unsigned char *bitmap;
	size_t index, size, chunk_index, i = 0;

	chunk = (Chunk *)(((size_t)ptr / CHUNK_SIZE) * CHUNK_SIZE);
	if (chunk->header.magic != MAGIC_BYTES)
		panic("Memory corruption: MAGIC not correct. Halting!\n");

	{
		LockGuard lg __attribute__((unused)) =
		    lock_write(&chunk->header.lock);

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
			LockGuard globallg __attribute__((unused)) =
			    lock_write(&__alloc_global_lock);
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

	munmap(chunk, CHUNK_SIZE);
}

void *cg_malloc(size_t size) {
	if (size < MAX_SLAB_SIZE) {
		size_t slab_size = calculate_slab_size(size);
		return alloc_slab(slab_size);
	} else {
		void *ptr;
		size_t aligned_size =
		    ((size + PAGE_SIZE - 1) / PAGE_SIZE) * PAGE_SIZE;
		ptr = alloc_aligned_memory(aligned_size, CHUNK_SIZE);
		return (void *)((size_t)ptr + HEADER_SIZE);
	}
}

void cg_free(void *ptr) {
	void *aligned_ptr;
	if (!ptr) return;
	aligned_ptr = (void *)((size_t)ptr - HEADER_SIZE);
	if ((size_t)aligned_ptr % CHUNK_SIZE == 0) {
		size_t actual_size;
		if (*(uint64_t *)((size_t)aligned_ptr + sizeof(uint64_t)) !=
		    MAGIC_BYTES)
			panic(
			    "Memory corruption: MAGIC not correct. Halting!\n");
		actual_size = *(size_t *)aligned_ptr;
		munmap(aligned_ptr, actual_size);
	} else {
		free_slab(ptr);
	}
}

#ifdef SET_MALLOC
void *malloc(size_t size) { return cg_malloc(size); }

void free(void *ptr) { cg_free(ptr); }

void *__wrap_malloc(size_t size) { return cg_malloc(size); }

void __wrap_free(void *ptr) { cg_free(ptr); }
#endif /* SET_MALLOC */
