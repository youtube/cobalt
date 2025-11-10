#include <stdlib.h>
#include <stdint.h>
#include "libc.h"
#include "lock.h"
#include "fork_impl.h"

#ifdef STARBOARD
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>

#include "build/build_config.h"
#include "starboard/common/log.h"
#endif  // STARBOARD

#define malloc __libc_malloc
#define realloc undef
#define free undef

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
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
#define LOCK(x)                                          \
    do {                                                 \
      SB_DCHECK(pthread_mutex_lock(&x) == 0); \
    } while (0)
#define UNLOCK(x)                    \
    do {                             \
      SB_DCHECK(pthread_mutex_unlock(&x) == 0); \
    } while (0)
#else   // !STARBOARD
static volatile int lock[1];
volatile int *const __atexit_lockptr = lock;
#endif // STARBOARD

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

#if !defined(ADDRESS_SANITIZER)
#if defined(USE_CUSTOM_MUSL_FINALIZE_SIGNATURE)
void __musl_cxa_finalize(void *dso)
#else  // defined(USE_CUSTOM_MUSL_FINALIZE_SIGNATURE)
void __cxa_finalize(void *dso)
#endif  // defined(USE_CUSTOM_MUSL_FINALIZE_SIGNATURE)
{
#ifdef STARBOARD
  __funcs_on_exit();
#endif  // STARBOARD
}
#endif  // !defined(ADDRESS_SANITIZER)

#if defined(USE_CUSTOM_MUSL_ATEXIT_SIGNATURE)
int __musl_cxa_atexit(void (*func)(void *), void *arg, void *dso)
#else  // defined(USE_CUSTOM_MUSL_ATEXIT_SIGNATURE)
int __cxa_atexit(void (*func)(void *), void *arg, void *dso)
#endif  // defined(USE_CUSTOM_MUSL_ATEXIT_SIGNATURE)
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
#if defined(USE_CUSTOM_MUSL_ATEXIT_SIGNATURE)
	return __musl_cxa_atexit(call, (void *)(uintptr_t)func, 0);
#else  // defined(USE_CUSTOM_MUSL_ATEXIT_SIGNATURE)
	return __cxa_atexit(call, (void *)(uintptr_t)func, 0);
#endif  // defined(USE_CUSTOM_MUSL_ATEXIT_SIGNATURE)
}
