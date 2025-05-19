#include <criterion/criterion.h>

#include "cgmalloc.h"

Test(cgmalloc, malloc) {
	char *test1 = cgmalloc(100);
	printf("addr=%zu\n", (size_t)test1);
	cgfree(test1);
}
