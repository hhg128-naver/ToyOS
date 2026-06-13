#ifndef SPINLOCK_H
#define SPINLOCK_H

#include <stdint.h>

/*
 * spinlock_t 구조체
 * volatile int lock: 락 상태 (0: 해제, 1: 획득)
 * uint32_t cpu: 현재 락을 소유한 CPU 코어 ID (디버깅용)
 */
typedef struct
{
	volatile int lock;
	uint32_t cpuID;
} spinlock_t;

void spinlock_init(spinlock_t* lock);
void spinlock_lock(spinlock_t* lock);
void spinlock_unlock(spinlock_t* lock);
uint64_t spinlock_lock_irqsave(spinlock_t* lock);
void spinlock_unlock_irqrestore(spinlock_t* lock, uint64_t flags);

#endif // SPINLOCK_H
