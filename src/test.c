#include <alloc.h>
#include <criterion/criterion.h>
#include <stdio.h>

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
