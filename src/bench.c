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

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "alloc.h"

int main() {
	struct timespec start, end;
	long size = 32000;
	long diff;

	void **arr;

	arr = cg_malloc(sizeof(void *) * size);

	clock_gettime(CLOCK_MONOTONIC, &start);
	for (int i = 0; i < size; i++) {
		arr[i] = malloc(1000);
	}
	for (int i = 0; i < size; i++) {
		free(arr[i]);
	}
	clock_gettime(CLOCK_MONOTONIC, &end);
	diff = (end.tv_sec - start.tv_sec) * 1000000000L +
	       (end.tv_nsec - start.tv_nsec);
	printf("malloc diff=%ld (%fns per iteration)\n", diff,
	       ((double)diff / (double)size));

	clock_gettime(CLOCK_MONOTONIC, &start);
	for (int i = 0; i < size; i++) {
		arr[i] = cg_malloc(100);
	}
	for (int i = 0; i < size; i++) {
		cg_free(arr[i]);
	}
	clock_gettime(CLOCK_MONOTONIC, &end);
	diff = (end.tv_sec - start.tv_sec) * 1000000000L +
	       (end.tv_nsec - start.tv_nsec);
	printf("cc_malloc diff=%ld (%fns per iteration)\n", diff,
	       ((double)diff / (double)size));

	cg_free(arr);

	return 0;
}
