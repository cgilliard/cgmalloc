#ifndef _LOCK_H__
#define _LOCK_H__

/* For uint64_t */
#include <stdint.h>

typedef uint64_t Lock;

#define LOCK_INIT 0

typedef struct {
	Lock *lock;
	unsigned char is_write;
} LockGuardImpl;

void lockguard_cleanup(LockGuardImpl *lg);

#define LockGuard LockGuardImpl __attribute__((cleanup(lockguard_cleanup)))

void lock_init(Lock *lock);
LockGuardImpl lock_read(Lock *lock);
LockGuardImpl lock_write(Lock *lock);

#endif /* _LOCK_H__ */
