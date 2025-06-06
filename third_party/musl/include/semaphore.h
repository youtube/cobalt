#ifndef _SEMAPHORE_H
#define _SEMAPHORE_H
#ifdef __cplusplus
extern "C" {
#endif

#include <features.h>

#define __NEED_time_t
#define __NEED_struct_timespec
#include <bits/alltypes.h>

#include <fcntl.h>

#define SEM_FAILED ((sem_t *)0)

#if defined(STARBOARD)
#include <stdint.h>

#define MUSL_SEM_MAX_SIZE 64
typedef union sem_t {
	uint8_t sem_buffer[MUSL_SEM_MAX_SIZE];

	// Guarantees alignment of the type to a void pointer.
	void* ptr;
  } sem_t;
#else
typedef struct {
	volatile int __val[4*sizeof(long)/sizeof(int)];
} sem_t;
#endif //  defined(STARBOARD)

int    sem_close(sem_t *);
int    sem_destroy(sem_t *);
int    sem_getvalue(sem_t *__restrict, int *__restrict);
int    sem_init(sem_t *, int, unsigned);
sem_t *sem_open(const char *, int, ...);
int    sem_post(sem_t *);
int    sem_timedwait(sem_t *__restrict, const struct timespec *__restrict);
int    sem_trywait(sem_t *);
int    sem_unlink(const char *);
int    sem_wait(sem_t *);

#if _REDIR_TIME64
__REDIR(sem_timedwait, __sem_timedwait_time64);
#endif

#ifdef __cplusplus
}
#endif
#endif
