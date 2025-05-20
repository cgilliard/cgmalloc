#include <stdio.h>
#include <time.h>

#include "alloc.h"

int main() {
	struct timespec res;
	int size = 1000;
	long nano_start, nano_end, diff;

	void *arr[size];
	clock_gettime(CLOCK_REALTIME, &res);
	nano_start = res.tv_nsec;
	for (int i = 0; i < size; i++) {
		arr[i] = alloc(100);
	}
	clock_gettime(CLOCK_REALTIME, &res);
	nano_end = res.tv_nsec;
	diff = nano_end - nano_end;
	printf("diff=%ld\n", diff);
	return 0;
}
