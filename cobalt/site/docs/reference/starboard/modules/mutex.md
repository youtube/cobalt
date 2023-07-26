---
layout: doc
title: "Starboard Module Reference: mutex.h"
---

Defines a mutually exclusive lock that can be used to coordinate with other
threads.

## Macros ##

### SB_MUTEX_MAX_SIZE ###

Max size of the SbMutex type.

## Enums ##

### SbMutexResult ###

Enumeration of possible results from acquiring a mutex.

#### Values ####

*   `kSbMutexAcquired`

    The mutex was acquired successfully.
*   `kSbMutexBusy`

    The mutex was not acquired because it was held by someone else.
*   `kSbMutexDestroyed`

    The mutex has already been destroyed.

## Typedefs ##

### SbMutex ###

An opaque handle to a mutex type with reserved memory buffer of size
SB_MUTEX_MAX_SIZE and aligned at void pointer type.

#### Definition ####

```
typedef union SbMutex  SbMutex
```

## Functions ##

### SbMutexAcquire ###

Acquires `mutex`, blocking indefinitely. The return value identifies the
acquisition result. SbMutexes are not reentrant, so a recursive acquisition
blocks forever.

`mutex`: The mutex to be acquired.

#### Declaration ####

```
SbMutexResult SbMutexAcquire(SbMutex *mutex)
```

### SbMutexAcquireTry ###

Acquires `mutex`, without blocking. The return value identifies the acquisition
result. SbMutexes are not reentrant, so a recursive acquisition has undefined
behavior.

`mutex`: The mutex to be acquired.

#### Declaration ####

```
SbMutexResult SbMutexAcquireTry(SbMutex *mutex)
```

### SbMutexCreate ###

Creates a new mutex. The return value indicates whether the function was able to
create a new mutex.

`out_mutex`: The handle to the newly created mutex.

#### Declaration ####

```
bool SbMutexCreate(SbMutex *out_mutex)
```

### SbMutexDestroy ###

Destroys a mutex. The return value indicates whether the destruction was
successful. Destroying a locked mutex results in undefined behavior.

`mutex`: The mutex to be invalidated.

#### Declaration ####

```
bool SbMutexDestroy(SbMutex *mutex)
```

### SbMutexIsSuccess ###

Indicates whether the given result is a success. A value of `true` indicates
that the mutex was acquired.

`result`: The result being checked.

#### Declaration ####

```
static bool SbMutexIsSuccess(SbMutexResult result)
```

### SbMutexRelease ###

Releases `mutex` held by the current thread. The return value indicates whether
the release was successful. Releases should always be successful if `mutex` is
held by the current thread.

`mutex`: The mutex to be released.

#### Declaration ####

```
bool SbMutexRelease(SbMutex *mutex)
```
