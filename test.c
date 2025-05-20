#include <criterion/criterion.h>
#include <stdio.h>

#include "alloc.h"

Test(alloc, alloc1) {
	char *test1 = cg_malloc(1000 * 1000 * 5);
	cg_free(test1);

	void *a = cg_malloc(10);

	for (int i = 0; i < 32000; i++) {
		char *test2 = cg_malloc(10);
		cr_assert(test2 != NULL);
		test2[0] = 'a';
		test2[1] = 'b';
		cg_free(test2);
	}

	cg_free(a);
}
