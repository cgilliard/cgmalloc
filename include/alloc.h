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

#ifndef _ALLOC_H__
#define _ALLOC_H__

/* For size_t */
#include <stddef.h>

/* To optimize this it's suggested to compile with -DPAGE_SIZE=VALUE. This
 * avoids the system from having to invoke the system call each time it needs
 * this value. */
#ifndef PAGE_SIZE
#define PAGE_SIZE (sysconf(_SC_PAGE_SIZE))
#endif /* PAGE_SIZE */

/* Size of chunks. All allocations are aligned to this size. This allows for us
 * to quickly find header info about a particular deallocation. This value must
 * be page aligned.
 */
#ifndef CHUNK_SIZE
#define CHUNK_SIZE (256 * 1024)
#endif /* CHUNK_SIZE */

/* Slab sizes are powers of 2 begining with 8. MAX_SLAB_SIZE is the largest slab
 * size we use. Requests for larger allocations allocation CHUNK_SIZE or more.
 */
#ifndef MAX_SLAB_SIZE
#define MAX_SLAB_SIZE 16384
#endif /* MAX_SLAB_SIZE */

void *cg_malloc(size_t size);
void cg_free(void *ptr);
void *cg_calloc(size_t n, size_t size);
void *cg_realloc(void *ptr, size_t size);

#endif /* _ALLOC_H__ */
