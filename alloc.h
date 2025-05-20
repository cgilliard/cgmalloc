#ifndef _CG_MALLOC_H__
#define _CG_MALLOC_H__

/* For size_t */
#include <stddef.h>

/* To optimize this declare your page size here. This avoids the system from
 * having to invoke the system call each time it needs this value. */
#define PAGE_SIZE (getpagesize())

/* Size of chunks. All allocations are aligned to this size. This allows for us
 * to quickly find header info about a particular deallocation. This value must
 * be page aligned.
 */
#define CHUNK_SIZE (4 * 1024 * 1024)

/* Slab sizes are powers of 2 begining with 8. MAX_SLAB_SIZE is the largest slab
 * size we use. Requests for larger allocations allocation CHUNK_SIZE or more.
 */
#define MAX_SLAB_SIZE 65536

void *cg_malloc(size_t size);
void cg_free(void *ptr);
void *cg_calloc(size_t n, size_t size);
void *cg_realloc(void *ptr, size_t size);

#endif /* _CG_MALLOC_H__ */
