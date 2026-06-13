#include "spinlock.h"
#include "kernel.h"

/*
 * 스핀락 초기화
 */
void spinlock_init(spinlock_t* lock)
{
	lock->lock = 0;
	lock->cpuID = 0; // 초기 소유 CPU 번호는 0 또는 정의되지 않음
}

/*
 * 스핀락 획득
 */
void spinlock_lock(spinlock_t* lock)
{
	// __sync_lock_test_and_set은 아토믹하게 lock->lock의 값을 1로 설정하고 이전 값을 반환합니다.
	// 이전 값이 1이면 이미 다른 스레드/CPU가 락을 소유하고 있는 것이므로 대기합니다.
	while (__sync_lock_test_and_set(&lock->lock, 1)) 
	{
		// CPU의 스핀 대기 루프 효율성을 위해 PAUSE 명령을 실행합니다.
		__builtin_ia32_pause();
	}
}

/*
 * 스핀락 해제
 */
void spinlock_unlock(spinlock_t* lock)
{
	// 아토믹하게 락 상태를 0으로 해제합니다.
	__sync_lock_release(&lock->lock);
}

/*
 * 인터럽트를 비활성화하고 스핀락 획득
 */
uint64_t spinlock_lock_irqsave(spinlock_t* lock)
{
	uint64_t flags = SaveAndDisableInterrupts();
	spinlock_lock(lock);
	return flags;
}

/*
 * 스핀락을 해제하고 이전 인터럽트 상태로 복구
 */
void spinlock_unlock_irqrestore(spinlock_t* lock, uint64_t flags)
{
	spinlock_unlock(lock);
	RestoreInterrupts(flags);
}
