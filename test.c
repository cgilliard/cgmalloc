#include <criterion/criterion.h>
#include <stdio.h>

#include "alloc.h"

Test(alloc, alloc1) {
	char *test1 = alloc(1000 * 1000 * 5);
	release(test1);

	void *a = alloc(10);

	for (int i = 0; i < 1000; i++) {
		char *test2 = alloc(10);
		printf("i=%i,test2=%lu\n", i, (size_t)test2);
		release(test2);
	}

	release(a);
}
