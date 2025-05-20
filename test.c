#include <criterion/criterion.h>
#include <stdio.h>

#include "alloc.h"

Test(alloc, alloc1) {
	char *test1 = alloc(1000 * 1000 * 5);
	release(test1);

	void *a = alloc(10);

	for (int i = 0; i < 1000; i++) {
		char *test2 = alloc(10);
		test2[0] = 'a';
		test2[1] = 'b';
		release(test2);
	}

	release(a);
}
