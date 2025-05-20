#include "lock.h"
/* For sched_yield */
#include <sched.h>

#define WFLAG (0x1UL << 63)
#define WREQUEST (0x1UL << 62)

void lock_init(Lock *lock) { *lock = 0; }

void lockguard_cleanup(LockGuardImpl *lg) {
	if (!lg->lock) return;
	if (lg->is_write) {
		__atomic_store_n(lg->lock, 0, __ATOMIC_RELEASE);
	} else {
		__atomic_fetch_sub(lg->lock, 1, __ATOMIC_RELEASE);
	}
}

LockGuardImpl lock_read(Lock *lock) {
	uint64_t state, desired;
	LockGuardImpl ret;
	do {
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
	uint64_t state, desired;
	LockGuardImpl ret;
	do {
		state = __atomic_load_n(lock, __ATOMIC_ACQUIRE) &
			~(WFLAG | WREQUEST);
		desired = state | WREQUEST;
	} while (!__atomic_compare_exchange_n(
	    lock, &state, desired, 0, __ATOMIC_RELEASE, __ATOMIC_RELAXED));

	desired = WFLAG;
	do {
		state = __atomic_load_n(lock, __ATOMIC_ACQUIRE);
		if (state != WREQUEST) sched_yield();
		if (state != WREQUEST) continue;

	} while (!__atomic_compare_exchange_n(
	    lock, &state, desired, 0, __ATOMIC_RELEASE, __ATOMIC_RELAXED));
	ret.lock = lock;
	ret.is_write = 1;
	return ret;
}
