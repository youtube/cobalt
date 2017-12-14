---
layout: doc
title: "Starboard Module Reference: thread.h"
---

Defines functionality related to thread creation and cleanup.

## Macros ##

### kSbThreadInvalidId ###

Well-defined constant value to mean "no thread ID."

### kSbThreadLocalKeyInvalid ###

Well-defined constant value to mean "no thread local key."

### kSbThreadNoAffinity ###

Well-defined constant value to mean "no affinity."

## Enums ##

### SbThreadPriority ###

A spectrum of thread priorities. Platforms map them appropriately to their own
priority system. Note that scheduling is platform-specific, and what these
priorities mean, if they mean anything at all, is also platform-specific.

In particular, several of these priority values can map to the same priority on
a given platform. The only guarantee is that each lower priority should be
treated less-than-or-equal-to a higher priority.

#### Values ####

*   `kSbThreadPriorityLowest`

    The lowest thread priority available on the current platform.
*   `kSbThreadPriorityLow`

    A lower-than-normal thread priority, if available on the current platform.
*   `kSbThreadPriorityNormal`

    Really, what is normal? You should spend time pondering that question more
    than you consider less-important things, but less than you think about more-
    important things.
*   `kSbThreadPriorityHigh`

    A higher-than-normal thread priority, if available on the current platform.
*   `kSbThreadPriorityHighest`

    The highest thread priority available on the current platform that isn't
    considered "real-time" or "time-critical," if those terms have any meaning
    on the current platform.
*   `kSbThreadPriorityRealTime`

    If the platform provides any kind of real-time or time-critical scheduling,
    this priority will request that treatment. Real-time scheduling generally
    means that the thread will have more consistency in scheduling than non-
    real-time scheduled threads, often by being more deterministic in how
    threads run in relation to each other. But exactly how being real-time
    affects the thread scheduling is platform-specific.

    For platforms where that is not offered, or otherwise not meaningful, this
    will just be the highest priority available in the platform's scheme, which
    may be the same as kSbThreadPriorityHighest.
*   `kSbThreadNoPriority`

    Well-defined constant value to mean "no priority." This means to use the
    default priority assignment method of that platform. This may mean to
    inherit the priority of the spawning thread, or it may mean a specific
    default priority, or it may mean something else, depending on the platform.

## Typedefs ##

### SbThreadAffinity ###

Type for thread core affinity. This generally will be a single cpu (or core or
hyperthread) identifier. Some platforms may not support affinity, and some may
have specific rules about how it must be used.

#### Definition ####

```
typedef int32_t SbThreadAffinity
```

### SbThreadEntryPoint ###

Function pointer type for SbThreadCreate. `context` is a pointer-sized bit of
data passed in from the calling thread.

#### Definition ####

```
typedef void*(* SbThreadEntryPoint)(void *context)
```

### SbThreadId ###

An ID type that is unique per thread.

#### Definition ####

```
typedef int32_t SbThreadId
```

### SbThreadLocalDestructor ###

Function pointer type for Thread-Local destructors.

#### Definition ####

```
typedef void(* SbThreadLocalDestructor)(void *value)
```

### SbThreadLocalKey ###

A handle to a thread-local key.

#### Definition ####

```
typedef SbThreadLocalKeyPrivate* SbThreadLocalKey
```

## Functions ##

### SbThreadCreate ###

Creates a new thread, which starts immediately.

*   If the function succeeds, the return value is a handle to the newly created
    thread.

*   If the function fails, the return value is `kSbThreadInvalid`.

`stack_size`: The amount of memory reserved for the thread. Set the value to `0`
to indicate that the default stack size should be used. `priority`: The thread's
priority. This value can be set to `kSbThreadNoPriority` to use the platform's
default priority. As examples, it could be set to a fixed, standard priority or
to a priority inherited from the thread that is calling SbThreadCreate(), or to
something else. `affinity`: The thread's affinity. This value can be set to
`kSbThreadNoAffinity` to use the platform's default affinity. `joinable`:
Indicates whether the thread can be joined (`true`) or should start out
"detached" (`false`). Note that for joinable threads, when you are done with the
thread handle, you must call `SbThreadJoin` to release system resources
associated with the thread. This is not necessary for detached threads, but
detached threads cannot be joined. `name`: A name used to identify the thread.
This value is used mainly for debugging, it can be `NULL`, and it might not be
used in production builds. `entry_point`: A pointer to a function that will be
executed on the newly created thread. `context`: This value will be passed to
the `entry_point` function.

#### Declaration ####

```
SbThread SbThreadCreate(int64_t stack_size, SbThreadPriority priority, SbThreadAffinity affinity, bool joinable, const char *name, SbThreadEntryPoint entry_point, void *context)
```

### SbThreadCreateLocalKey ###

Creates and returns a new, unique key for thread local data. If the function
does not succeed, the function returns `kSbThreadLocalKeyInvalid`.

If `destructor` is specified, it will be called in the owning thread, and only
in the owning thread, when the thread exits. In that case, it is called on the
local value associated with the key in the current thread as long as the local
value is not NULL.

`destructor`: A pointer to a function. The value may be NULL if no clean up is
needed.

#### Declaration ####

```
SbThreadLocalKey SbThreadCreateLocalKey(SbThreadLocalDestructor destructor)
```

### SbThreadDestroyLocalKey ###

Destroys thread local data for the specified key. The function is a no-op if the
key is invalid (kSbThreadLocalKeyInvalid`) or has already been destroyed. This
function does NOT call the destructor on any stored values.

`key`: The key for which to destroy thread local data.

#### Declaration ####

```
void SbThreadDestroyLocalKey(SbThreadLocalKey key)
```

### SbThreadDetach ###

Detaches `thread`, which prevents it from being joined. This is sort of like a
non-blocking join. This function is a no-op if the thread is already detached or
if the thread is already being joined by another thread.

`thread`: The thread to be detached.

#### Declaration ####

```
void SbThreadDetach(SbThread thread)
```

### SbThreadGetCurrent ###

Returns the handle of the currently executing thread.

#### Declaration ####

```
SbThread SbThreadGetCurrent()
```

### SbThreadGetId ###

Returns the Thread ID of the currently executing thread.

#### Declaration ####

```
SbThreadId SbThreadGetId()
```

### SbThreadGetLocalValue ###

Returns the pointer-sized value for `key` in the currently executing thread's
local storage. Returns `NULL` if key is `kSbThreadLocalKeyInvalid` or if the key
has already been destroyed.

`key`: The key for which to return the value.

#### Declaration ####

```
void* SbThreadGetLocalValue(SbThreadLocalKey key)
```

### SbThreadGetName ###

Returns the debug name of the currently executing thread.

#### Declaration ####

```
void SbThreadGetName(char *buffer, int buffer_size)
```

### SbThreadIsCurrent ###

Returns whether `thread` is the current thread.

`thread`: The thread to check.

#### Declaration ####

```
static bool SbThreadIsCurrent(SbThread thread)
```

### SbThreadIsEqual ###

Indicates whether `thread1` and `thread2` refer to the same thread.

`thread1`: The first thread to compare. `thread2`: The second thread to compare.

#### Declaration ####

```
bool SbThreadIsEqual(SbThread thread1, SbThread thread2)
```

### SbThreadIsValid ###

Returns whether the given thread handle is valid.

#### Declaration ####

```
static bool SbThreadIsValid(SbThread thread)
```

### SbThreadIsValidAffinity ###

Returns whether the given thread affinity is valid.

#### Declaration ####

```
static bool SbThreadIsValidAffinity(SbThreadAffinity affinity)
```

### SbThreadIsValidId ###

Returns whether the given thread ID is valid.

#### Declaration ####

```
static bool SbThreadIsValidId(SbThreadId id)
```

### SbThreadIsValidLocalKey ###

Returns whether the given thread local variable key is valid.

#### Declaration ####

```
static bool SbThreadIsValidLocalKey(SbThreadLocalKey key)
```

### SbThreadIsValidPriority ###

Returns whether the given thread priority is valid.

#### Declaration ####

```
static bool SbThreadIsValidPriority(SbThreadPriority priority)
```

### SbThreadJoin ###

Joins the thread on which this function is called with joinable `thread`. This
function blocks the caller until the designated thread exits, and then cleans up
that thread's resources. The cleanup process essentially detaches thread.

The return value is `true` if the function is successful and `false` if `thread`
is invalid or detached.

Each joinable thread can only be joined once and must be joined to be fully
cleaned up. Once SbThreadJoin is called, the thread behaves as if it were
detached to all threads other than the joining thread.

`thread`: The thread to which the current thread will be joined. The `thread`
must have been created with SbThreadCreate. `out_return`: If this is not `NULL`,
then the SbThreadJoin function populates it with the return value of the
thread's `main` function.

#### Declaration ####

```
bool SbThreadJoin(SbThread thread, void **out_return)
```

### SbThreadSetLocalValue ###

Sets the pointer-sized value for `key` in the currently executing thread's local
storage. The return value indicates whether `key` is valid and has not already
been destroyed.

`key`: The key for which to set the key value. `value`: The new pointer-sized
key value.

#### Declaration ####

```
bool SbThreadSetLocalValue(SbThreadLocalKey key, void *value)
```

### SbThreadSetName ###

Sets the debug name of the currently executing thread by copying the specified
name string.

`name`: The name to assign to the thread.

#### Declaration ####

```
void SbThreadSetName(const char *name)
```

### SbThreadSleep ###

Sleeps the currently executing thread.

`duration`: The minimum amount of time, in microseconds, that the currently
executing thread should sleep. The function is a no-op if this value is negative
or `0`.

#### Declaration ####

```
void SbThreadSleep(SbTime duration)
```

### SbThreadYield ###

Yields the currently executing thread, so another thread has a chance to run.

#### Declaration ####

```
void SbThreadYield()
```

