#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "alloc.h"

int main() {
	struct timespec start, end;
	int size = 32000;
	long diff;

	void *arr[size];
	clock_gettime(CLOCK_MONOTONIC, &start);
	for (int i = 0; i < size; i++) {
		arr[i] = malloc(100);
	}
	for (int i = 0; i < size; i++) {
		free(arr[i]);
	}
	clock_gettime(CLOCK_MONOTONIC, &end);
	diff = (end.tv_sec - start.tv_sec) * 1000000000L +
	       (end.tv_nsec - start.tv_nsec);
	printf("diff=%ld (%fns per iteration)\n", diff,
	       ((double)diff / (double)size));

	return 0;
}
