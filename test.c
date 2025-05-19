#include <criterion/criterion.h>

#include "cgmalloc.h"

Test(cgmalloc, malloc) {
	char *test1 = cgmalloc(1000 * 1000 * 5);
	cgfree(test1);

	for (int i = 0; i < 1000; i++) {
		char *test2 = cgmalloc(10);
		// printf("i=%i,test2=%lu\n", i, (size_t)test2);
	}

	// cgfree(test2);
}
