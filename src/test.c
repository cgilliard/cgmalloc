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
#include <criterion/criterion.h>
#include <stdio.h>

// test functions
int calculate_slab_size(size_t value);

Test(alloc, alloc1) {
	size_t last_address;
	char *test1 = cg_malloc(1000 * 1000 * 5);
	cg_free(test1);

	void *a = cg_malloc(10);

	last_address = 0;
	for (int i = 0; i < 32000; i++) {
		char *test2 = cg_malloc(10);
		// after first loop, we should just be reusing addresses
		if (last_address)
			cr_assert_eq((size_t)test2, last_address);
		else
			cr_assert(a != test2);
		cr_assert(test2 != NULL);
		test2[0] = 'a';
		test2[1] = 'b';
		cg_free(test2);
		last_address = (size_t)test2;
	}

	cg_free(a);

	// use a different size
	int size = 1000;
	char *values[size];
	last_address = 0;
	for (int i = 0; i < size; i++) {
		values[i] = cg_malloc(32);
		// after first loop, we should be incrementing by 32 with each
		// address
		if (last_address)
			cr_assert_eq((size_t)values[i], last_address + 32);
		last_address = (size_t)values[i];
	}

	for (int i = 0; i < size; i++) {
		cg_free(values[i]);
	}
}

Test(alloc, slab_sizes) {
	cr_assert_eq(calculate_slab_size(1), 8);
	cr_assert_eq(calculate_slab_size(8), 8);
	for (size_t i = 9; i <= 16; i++)
		cr_assert_eq(calculate_slab_size(i), 16);
	for (size_t i = 17; i <= 32; i++)
		cr_assert_eq(calculate_slab_size(i), 32);
	for (size_t i = 33; i <= 64; i++)
		cr_assert_eq(calculate_slab_size(i), 64);

	for (size_t i = 65; i <= 128; i++)
		cr_assert_eq(calculate_slab_size(i), 128);

	for (size_t i = 129; i <= 256; i++)
		cr_assert_eq(calculate_slab_size(i), 256);

	for (size_t i = 257; i <= 512; i++)
		cr_assert_eq(calculate_slab_size(i), 512);

	for (size_t i = 513; i <= 1024; i++)
		cr_assert_eq(calculate_slab_size(i), 1024);

	for (size_t i = 1025; i <= 2048; i++)
		cr_assert_eq(calculate_slab_size(i), 2048);
}

Test(alloc, slab_impl) {
	int size = 127;
	char *values[size];
	size_t last_address = 0;
	for (int i = 0; i < size; i++) {
		values[i] = cg_malloc(2048);
		if (last_address)
			cr_assert_eq((size_t)values[i], last_address + 2048);
		last_address = (size_t)values[i];
	}

	void *next = cg_malloc(2048);
	// This creates a new mmap so the address will not be last + 2048
	cr_assert((size_t)next != last_address + 2048);

	void *next2 = cg_malloc(2040);	// still in the 2048 block size
	cr_assert_eq((size_t)next2, (size_t)next + 2048);

	cg_free(next);
	cg_free(next2);

	for (int i = 0; i < size; i++) {
		cg_free(values[i]);
	}
}

Test(alloc, slab_release) {
	int size = 127;
	char *values[size];
	size_t last_address = 0;
	for (int i = 0; i < size; i++) {
		values[i] = cg_malloc(2048);
		if (last_address)
			cr_assert_eq((size_t)values[i], last_address + 2048);
		last_address = (size_t)values[i];
	}

	// free several slabs
	cg_free(values[18]);
	cg_free(values[125]);
	cg_free(values[15]);

	// ensure last pointer is being used and gets us the correct bits.
	void *next = cg_malloc(2048);
	void *next2 = cg_malloc(2048);
	void *next3 = cg_malloc(2048);

	cr_assert_eq((size_t)next, (size_t)values[15]);
	cr_assert_eq((size_t)next2, (size_t)values[18]);
	cr_assert_eq((size_t)next3, (size_t)values[125]);

	values[15] = next;
	values[18] = next2;
	values[125] = next3;

	for (int i = 0; i < size; i++) {
		cg_free(values[i]);
	}
}

Test(alloc, multi_chunk_search) {
	int size = 1000;
	char *values[size];
	size_t last_address = 0;
	for (int i = 0; i < size; i++) {
		values[i] = cg_malloc(2048);
	}

	// free several slabs
	cg_free(values[418]);
	cg_free(values[825]);
	cg_free(values[15]);

	void *next = cg_malloc(2048);
	void *next2 = cg_malloc(2048);
	void *next3 = cg_malloc(2048);

	cr_assert_eq((size_t)next, (size_t)values[15]);
	cr_assert_eq((size_t)next2, (size_t)values[418]);
	cr_assert_eq((size_t)next3, (size_t)values[825]);

	values[15] = next;
	values[825] = next2;
	values[418] = next3;

	for (int i = 0; i < size; i++) {
		cg_free(values[i]);
	}
}

Test(alloc, larger_allocations) {
	size_t max = 16384 * 3;
	for (size_t i = 2049; i < max; i++) {
		char *ptr = cg_malloc(i);
		cr_assert(ptr);
		for (size_t j = 0; j < 10; j++) ptr[j] = 'x';
		for (size_t j = 0; j < 10; j++) cr_assert_eq(ptr[j], 'x');
		cg_free(ptr);
	}
}

Test(alloc, test_realloc) {
	void *tmp;

	void *slab1 = cg_malloc(8);
	void *slab2 = cg_malloc(16);
	void *slab3 = cg_malloc(32);
	void *slab4 = cg_malloc(64);
	void *slab5 = cg_malloc(128);

	tmp = cg_realloc(slab1, 16);
	cr_assert_eq((size_t)tmp, (size_t)slab2 + 16);
	slab1 = tmp;

	tmp = cg_realloc(slab1, 32);
	cr_assert_eq((size_t)tmp, (size_t)slab3 + 32);
	slab1 = tmp;

	tmp = cg_realloc(slab1, 64);
	cr_assert_eq((size_t)tmp, (size_t)slab4 + 64);
	slab1 = tmp;

	tmp = cg_realloc(slab1, 128);
	cr_assert_eq((size_t)tmp, (size_t)slab5 + 128);
	slab1 = tmp;

	tmp = cg_realloc(slab1, 1024 * 1024 * 6);
	/* Should be a CHUNK_SIZE aligned + 16 pointer */
	cr_assert_eq(((size_t)tmp - 16) % CHUNK_SIZE, 0);
	slab1 = tmp;

	/* now go down */
	tmp = cg_realloc(slab1, 16);
	cr_assert_eq((size_t)tmp, (size_t)slab2 + 16);
	slab1 = tmp;

	cg_free(slab1);
	cg_free(slab2);
	cg_free(slab3);
	cg_free(slab4);
	cg_free(slab5);
}

Test(alloc, test_calloc) {
	unsigned char *tmp;
	tmp = cg_calloc(10, 10);
	for (int i = 0; i < 100; i++) cr_assert_eq(tmp[i], 0);
	cg_free(tmp);
}
