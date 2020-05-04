#include <stdlib.h>
#include <stdint.h>
#include "libc.h"

#ifdef STARBOARD
#include "starboard/common/log.h"
#include "starboard/mutex.h"
#include "starboard/thread_types.h"
#include "starboard/types.h"
#endif  // STARBOARD

/* Ensure that at least 32 atexit handlers can be registered without malloc */
#define COUNT 32

static struct fl
{
	struct fl *next;
	void (*f[COUNT])(void *);
	void *a[COUNT];
} builtin, *head;

static int slot;
#ifdef STARBOARD
static SbMutex lock = SB_MUTEX_INITIALIZER;
#define LOCK(x)                                          \
    do {                                                 \
      SB_DCHECK(SbMutexAcquire(&x) == kSbMutexAcquired); \
    } while (0)
#define UNLOCK(x)                    \
    do {                             \
      SB_DCHECK(SbMutexRelease(&x)); \
    } while (0)
#else   // !STARBOARD
static volatile int lock[1];
#endif  // STARBOARD

void __funcs_on_exit()
{
	void (*func)(void *), *arg;
	LOCK(lock);
	for (; head; head=head->next, slot=COUNT) while(slot-->0) {
		func = head->f[slot];
		arg = head->a[slot];
		UNLOCK(lock);
		func(arg);
		LOCK(lock);
	}
}

void __cxa_finalize(void *dso)
{
#ifdef STARBOARD
  __funcs_on_exit();
#endif  // STARBOARD
}

int __cxa_atexit(void (*func)(void *), void *arg, void *dso)
{
	LOCK(lock);

	/* Defer initialization of head so it can be in BSS */
	if (!head) head = &builtin;

	/* If the current function list is full, add a new one */
	if (slot==COUNT) {
		struct fl *new_fl = calloc(sizeof(struct fl), 1);
		if (!new_fl) {
			UNLOCK(lock);
			return -1;
		}
		new_fl->next = head;
		head = new_fl;
		slot = 0;
	}

	/* Append function to the list. */
	head->f[slot] = func;
	head->a[slot] = arg;
	slot++;

	UNLOCK(lock);
	return 0;
}

static void call(void *p)
{
	((void (*)(void))(uintptr_t)p)();
}

int atexit(void (*func)(void))
{
	return __cxa_atexit(call, (void *)(uintptr_t)func, 0);
}
