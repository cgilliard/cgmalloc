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
