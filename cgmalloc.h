#ifndef _CG_MALLOC_H__
#define _CG_MALLOC_H__

/* For size_t */
#include <stddef.h>

#define CHUNK_SIZE (4 * 1024 * 1024) /* 4 mb */
#define MAX_SLAB_SIZE 0

void *cgmalloc(size_t size);
void cgfree(void *ptr);
void *cgcalloc(size_t n, size_t size);
void *cgrealloc(void *ptr, size_t size);

#endif /* _CG_MALLOC_H__ */
