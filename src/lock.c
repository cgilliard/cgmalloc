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

#include <lock.h>

/* For sched_yield */
#include <sched.h>

#define WFLAG (0x1UL << 63)
#define WREQUEST (0x1UL << 62)

void lock_init(Lock *lock) { *lock = 0; }

void lockguard_cleanup(LockGuardImpl *lg) {
	if (!lg->lock)
		return;
	else if (lg->is_write)
		__atomic_store_n(lg->lock, 0, __ATOMIC_RELEASE);
	else
		__atomic_fetch_sub(lg->lock, 1, __ATOMIC_RELEASE);
}

LockGuardImpl lock_read(Lock *lock) {
	uint64_t state, desired, do_yield = 0;
	LockGuardImpl ret;
	do {
		if (do_yield++) sched_yield();
		state = __atomic_load_n(lock, __ATOMIC_ACQUIRE) &
			~(WFLAG | WREQUEST);
		desired = state + 1;
	} while (!__atomic_compare_exchange_n(
	    lock, &state, desired, 0, __ATOMIC_RELEASE, __ATOMIC_RELAXED));
	ret.lock = lock;
	ret.is_write = 0;
	return ret;
}

LockGuardImpl lock_write(Lock *lock) {
	uint64_t state, desired, do_yield = 0;
	LockGuardImpl ret;
	do {
		if (do_yield++) sched_yield();
		state = __atomic_load_n(lock, __ATOMIC_ACQUIRE) &
			~(WFLAG | WREQUEST);
		desired = state | WREQUEST;
	} while (!__atomic_compare_exchange_n(
	    lock, &state, desired, 0, __ATOMIC_RELEASE, __ATOMIC_RELAXED));

	desired = WFLAG;
	do {
		state = __atomic_load_n(lock, __ATOMIC_ACQUIRE);
		if (state != WREQUEST) {
			sched_yield();
			continue;
		}

	} while (!__atomic_compare_exchange_n(
	    lock, &state, desired, 0, __ATOMIC_RELEASE, __ATOMIC_RELAXED));
	ret.lock = lock;
	ret.is_write = 1;
	return ret;
}
