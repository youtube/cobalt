Project: /youtube/cobalt/_project.yaml
Book: /youtube/cobalt/_book.yaml

# Starboard Module Reference: `thread.h`

Defines functionality related to thread creation and cleanup.

## Macros

### kSbThreadInvalidId

Well-defined constant value to mean "no thread ID."

## Enums

### SbThreadPriority

A spectrum of thread priorities. Platforms map them appropriately to their own
priority system. Note that scheduling is platform-specific, and what these
priorities mean, if they mean anything at all, is also platform-specific.

In particular, several of these priority values can map to the same priority on
a given platform. The only guarantee is that each lower priority should be
treated less-than-or-equal-to a higher priority.

#### Values

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

## Typedefs

### SbThreadId

An ID type that is unique per thread.

#### Definition

```
typedef int32_t SbThreadId
```

## Functions

### SbThreadGetId

Returns the Thread ID of the currently executing thread.

#### Declaration

```
SbThreadId SbThreadGetId()
```

### SbThreadGetPriority

Get the thread priority of the current thread.

#### Declaration

```
bool SbThreadGetPriority(SbThreadPriority *priority)
```

### SbThreadIsValidId

Returns whether the given thread ID is valid.

#### Declaration

```
static bool SbThreadIsValidId(SbThreadId id)
```

### SbThreadIsValidPriority

Returns whether the given thread priority is valid.

#### Declaration

```
static bool SbThreadIsValidPriority(SbThreadPriority priority)
```

### SbThreadSetPriority

Set the thread priority of the current thread.

#### Declaration

```
bool SbThreadSetPriority(SbThreadPriority priority)
```
