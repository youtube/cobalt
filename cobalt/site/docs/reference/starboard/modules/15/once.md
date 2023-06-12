---
layout: doc
title: "Starboard Module Reference: once.h"
---

Onces represent initializations that should only ever happen once per process,
in a thread-safe way.

## Macros ##

### SB_ONCE_MAX_SIZE ###

Max size of the SbOnceControl type.

## Typedefs ##

### SbOnceControl ###

An opaque handle to a once control type with reserved memory buffer of size
SB_ONCE_MAX_SIZE and aligned at void pointer type.

#### Definition ####

```
typedef union SbOnceControl  SbOnceControl
```

### SbOnceInitRoutine ###

Function pointer type for methods that can be called via the SbOnce() system.

#### Definition ####

```
typedef void(* SbOnceInitRoutine) (void)
```

## Functions ##

### SbOnce ###

Thread-safely runs `init_routine` only once.

*   If this `once_control` has not run a function yet, this function runs
    `init_routine` in a thread-safe way and then returns `true`.

*   If SbOnce() was called with `once_control` before, the function returns
    `true` immediately.

*   If `once_control` or `init_routine` is invalid, the function returns
    `false`.

#### Declaration ####

```
bool SbOnce(SbOnceControl *once_control, SbOnceInitRoutine init_routine)
```
