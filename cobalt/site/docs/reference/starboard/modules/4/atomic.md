---
layout: doc
title: "Starboard Module Reference: atomic.h"
---

Defines a set of atomic integer operations that can be used as lightweight
synchronization or as building blocks for heavier synchronization primitives.
Their use is very subtle and requires detailed understanding of the behavior of
supported architectures, so their direct use is not recommended except when
rigorously deemed absolutely necessary for performance reasons.

## Typedefs ##

### SbAtomic64 ###

64-bit atomic operations (only available on 64-bit processors).

#### Definition ####

```
typedef int64_t SbAtomic64
```

### SbAtomicPtr ###

Pointer-sized atomic operations. Forwards to either 32-bit or 64-bit functions
as appropriate.

#### Definition ####

```
typedef SbAtomic64 SbAtomicPtr
```

## Functions ##

### SbAtomicAcquire_CompareAndSwap ###

These following lower-level operations are typically useful only to people
implementing higher-level synchronization operations like spinlocks, mutexes,
and condition-variables. They combine CompareAndSwap(), a load, or a store with
appropriate memory-ordering instructions. "Acquire" operations ensure that no
later memory access can be reordered ahead of the operation. "Release"
operations ensure that no previous memory access can be reordered after the
operation. "Barrier" operations have both "Acquire" and "Release" semantics. A
SbAtomicMemoryBarrier() has "Barrier" semantics, but does no memory access.

#### Declaration ####

```
static SbAtomic32 SbAtomicAcquire_CompareAndSwap(volatile SbAtomic32 *ptr, SbAtomic32 old_value, SbAtomic32 new_value)
```

### SbAtomicBarrier_Increment ###

Same as SbAtomicNoBarrier_Increment, but with a memory barrier.

#### Declaration ####

```
static SbAtomic32 SbAtomicBarrier_Increment(volatile SbAtomic32 *ptr, SbAtomic32 increment)
```

### SbAtomicNoBarrier_CompareAndSwap ###

Atomically execute: result = *ptr; if (*ptr == old_value) *ptr = new_value;
return result;

I.e., replace "*ptr" with "new_value" if "*ptr" used to be "old_value". Always
return the old value of "*ptr"

This routine implies no memory barriers.

#### Declaration ####

```
static SbAtomic32 SbAtomicNoBarrier_CompareAndSwap(volatile SbAtomic32 *ptr, SbAtomic32 old_value, SbAtomic32 new_value)
```

### SbAtomicNoBarrier_Exchange ###

Atomically store new_value into *ptr, returning the previous value held in *ptr.
This routine implies no memory barriers.

#### Declaration ####

```
static SbAtomic32 SbAtomicNoBarrier_Exchange(volatile SbAtomic32 *ptr, SbAtomic32 new_value)
```

### SbAtomicNoBarrier_Increment ###

Atomically increment *ptr by "increment". Returns the new value of *ptr with the
increment applied. This routine implies no memory barriers.

#### Declaration ####

```
static SbAtomic32 SbAtomicNoBarrier_Increment(volatile SbAtomic32 *ptr, SbAtomic32 increment)
```

