---
layout: doc
title: "Starboard Module Reference: atomic.h"
---

Defines a set of atomic integer operations that can be used as lightweight
synchronization or as building blocks for heavier synchronization primitives.
Their use is very subtle and requires detailed understanding of the behavior
of supported architectures, so their direct use is not recommended except
when rigorously deemed absolutely necessary for performance reasons.

## Functions

### SbAtomicAcquire_CompareAndSwap

**Declaration**

```
static SbAtomic32 SbAtomicAcquire_CompareAndSwap(volatile SbAtomic32* ptr,
                                                 SbAtomic32 old_value,
                                                 SbAtomic32 new_value);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>volatile SbAtomic32*</code><br>
        <code>ptr</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>SbAtomic32</code><br>
        <code>old_value</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>SbAtomic32</code><br>
        <code>new_value</code></td>
    <td> </td>
  </tr>
</table>

### SbAtomicAcquire_CompareAndSwap64

**Declaration**

```
static SbAtomic64 SbAtomicAcquire_CompareAndSwap64(volatile SbAtomic64* ptr,
                                                   SbAtomic64 old_value,
                                                   SbAtomic64 new_value);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>volatile SbAtomic64*</code><br>
        <code>ptr</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>SbAtomic64</code><br>
        <code>old_value</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>SbAtomic64</code><br>
        <code>new_value</code></td>
    <td> </td>
  </tr>
</table>

### SbAtomicAcquire_Load

**Declaration**

```
static SbAtomic32 SbAtomicAcquire_Load(volatile const SbAtomic32* ptr);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>volatile const SbAtomic32*</code><br>
        <code>ptr</code></td>
    <td> </td>
  </tr>
</table>

### SbAtomicAcquire_Load64

**Declaration**

```
static SbAtomic64 SbAtomicAcquire_Load64(volatile const SbAtomic64* ptr);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>volatile const SbAtomic64*</code><br>
        <code>ptr</code></td>
    <td> </td>
  </tr>
</table>

### SbAtomicAcquire_Store

**Declaration**

```
static void SbAtomicAcquire_Store(volatile SbAtomic32* ptr, SbAtomic32 value);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>volatile SbAtomic32*</code><br>
        <code>ptr</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>SbAtomic32</code><br>
        <code>value</code></td>
    <td> </td>
  </tr>
</table>

### SbAtomicAcquire_Store64

**Declaration**

```
static void SbAtomicAcquire_Store64(volatile SbAtomic64* ptr, SbAtomic64 value);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>volatile SbAtomic64*</code><br>
        <code>ptr</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>SbAtomic64</code><br>
        <code>value</code></td>
    <td> </td>
  </tr>
</table>

### SbAtomicBarrier_Increment

**Declaration**

```
static SbAtomic32 SbAtomicBarrier_Increment(volatile SbAtomic32* ptr,
                                            SbAtomic32 increment);
// These following lower-level operations are typically useful only to people
// implementing higher-level synchronization operations like spinlocks, mutexes,
// and condition-variables.  They combine CompareAndSwap(), a load, or a store
// with appropriate memory-ordering instructions.  "Acquire" operations ensure
// that no later memory access can be reordered ahead of the operation.
// "Release" operations ensure that no previous memory access can be reordered
// after the operation.  "Barrier" operations have both "Acquire" and "Release"
// semantics.  A SbAtomicMemoryBarrier() has "Barrier" semantics, but does no
// memory access.
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>volatile SbAtomic32*</code><br>
        <code>ptr</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>SbAtomic32</code><br>
        <code>increment</code></td>
    <td> </td>
  </tr>
</table>

### SbAtomicBarrier_Increment64

**Declaration**

```
static SbAtomic64 SbAtomicBarrier_Increment64(volatile SbAtomic64* ptr,
                                              SbAtomic64 increment);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>volatile SbAtomic64*</code><br>
        <code>ptr</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>SbAtomic64</code><br>
        <code>increment</code></td>
    <td> </td>
  </tr>
</table>

### SbAtomicMemoryBarrier

**Declaration**

```
static void SbAtomicMemoryBarrier();
```

### SbAtomicNoBarrier_CompareAndSwap

**Description**

Atomically execute:
result = *ptr;
if (*ptr == old_value)
*ptr = new_value;
return result;<br>
I.e., replace "*ptr" with "new_value" if "*ptr" used to be "old_value".
Always return the old value of "*ptr"<br>
This routine implies no memory barriers.

**Declaration**

```
static SbAtomic32 SbAtomicNoBarrier_CompareAndSwap(volatile SbAtomic32* ptr,
                                                   SbAtomic32 old_value,
                                                   SbAtomic32 new_value);
// Atomically store new_value into *ptr, returning the previous value held in
// *ptr.  This routine implies no memory barriers.
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>volatile SbAtomic32*</code><br>
        <code>ptr</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>SbAtomic32</code><br>
        <code>old_value</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>SbAtomic32</code><br>
        <code>new_value</code></td>
    <td> </td>
  </tr>
</table>

### SbAtomicNoBarrier_CompareAndSwap64

**Declaration**

```
static SbAtomic64 SbAtomicNoBarrier_CompareAndSwap64(volatile SbAtomic64* ptr,
                                                     SbAtomic64 old_value,
                                                     SbAtomic64 new_value);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>volatile SbAtomic64*</code><br>
        <code>ptr</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>SbAtomic64</code><br>
        <code>old_value</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>SbAtomic64</code><br>
        <code>new_value</code></td>
    <td> </td>
  </tr>
</table>

### SbAtomicNoBarrier_Exchange

**Declaration**

```
static SbAtomic32 SbAtomicNoBarrier_Exchange(volatile SbAtomic32* ptr,
                                             SbAtomic32 new_value);
// Atomically increment *ptr by "increment".  Returns the new value of
// *ptr with the increment applied.  This routine implies no memory barriers.
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>volatile SbAtomic32*</code><br>
        <code>ptr</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>SbAtomic32</code><br>
        <code>new_value</code></td>
    <td> </td>
  </tr>
</table>

### SbAtomicNoBarrier_Exchange64

**Declaration**

```
static SbAtomic64 SbAtomicNoBarrier_Exchange64(volatile SbAtomic64* ptr,
                                               SbAtomic64 new_value);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>volatile SbAtomic64*</code><br>
        <code>ptr</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>SbAtomic64</code><br>
        <code>new_value</code></td>
    <td> </td>
  </tr>
</table>

### SbAtomicNoBarrier_Increment

**Declaration**

```
static SbAtomic32 SbAtomicNoBarrier_Increment(volatile SbAtomic32* ptr,
                                              SbAtomic32 increment);
// Same as SbAtomicNoBarrier_Increment, but with a memory barrier.
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>volatile SbAtomic32*</code><br>
        <code>ptr</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>SbAtomic32</code><br>
        <code>increment</code></td>
    <td> </td>
  </tr>
</table>

### SbAtomicNoBarrier_Increment64

**Declaration**

```
static SbAtomic64 SbAtomicNoBarrier_Increment64(volatile SbAtomic64* ptr,
                                                SbAtomic64 increment);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>volatile SbAtomic64*</code><br>
        <code>ptr</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>SbAtomic64</code><br>
        <code>increment</code></td>
    <td> </td>
  </tr>
</table>

### SbAtomicNoBarrier_Load

**Declaration**

```
static SbAtomic32 SbAtomicNoBarrier_Load(volatile const SbAtomic32* ptr);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>volatile const SbAtomic32*</code><br>
        <code>ptr</code></td>
    <td> </td>
  </tr>
</table>

### SbAtomicNoBarrier_Load64

**Declaration**

```
static SbAtomic64 SbAtomicNoBarrier_Load64(volatile const SbAtomic64* ptr);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>volatile const SbAtomic64*</code><br>
        <code>ptr</code></td>
    <td> </td>
  </tr>
</table>

### SbAtomicNoBarrier_Store

**Declaration**

```
static void SbAtomicNoBarrier_Store(volatile SbAtomic32* ptr, SbAtomic32 value);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>volatile SbAtomic32*</code><br>
        <code>ptr</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>SbAtomic32</code><br>
        <code>value</code></td>
    <td> </td>
  </tr>
</table>

### SbAtomicNoBarrier_Store64

**Declaration**

```
static void SbAtomicNoBarrier_Store64(volatile SbAtomic64* ptr,
                                      SbAtomic64 value);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>volatile SbAtomic64*</code><br>
        <code>ptr</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>SbAtomic64</code><br>
        <code>value</code></td>
    <td> </td>
  </tr>
</table>

### SbAtomicPtr

**Declaration**

```
static SB_C_FORCE_INLINE SbAtomicPtr
SbAtomicRelease_LoadPtr(volatile const SbAtomicPtr* ptr) {
#if SB_HAS(64_BIT_POINTERS)
  return SbAtomicRelease_Load64(ptr);
#else
  return SbAtomicRelease_Load(ptr);
#endif
}
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbAtomicRelease_LoadPtr(volatile const SbAtomicPtr*</code><br>
        <code>ptr</code></td>
    <td> </td>
  </tr>
</table>

### SbAtomicRelease_CompareAndSwap

**Declaration**

```
static SbAtomic32 SbAtomicRelease_CompareAndSwap(volatile SbAtomic32* ptr,
                                                 SbAtomic32 old_value,
                                                 SbAtomic32 new_value);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>volatile SbAtomic32*</code><br>
        <code>ptr</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>SbAtomic32</code><br>
        <code>old_value</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>SbAtomic32</code><br>
        <code>new_value</code></td>
    <td> </td>
  </tr>
</table>

### SbAtomicRelease_CompareAndSwap64

**Declaration**

```
static SbAtomic64 SbAtomicRelease_CompareAndSwap64(volatile SbAtomic64* ptr,
                                                   SbAtomic64 old_value,
                                                   SbAtomic64 new_value);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>volatile SbAtomic64*</code><br>
        <code>ptr</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>SbAtomic64</code><br>
        <code>old_value</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>SbAtomic64</code><br>
        <code>new_value</code></td>
    <td> </td>
  </tr>
</table>

### SbAtomicRelease_Load

**Declaration**

```
static SbAtomic32 SbAtomicRelease_Load(volatile const SbAtomic32* ptr);
// 64-bit atomic operations (only available on 64-bit processors).
#if SB_HAS(64_BIT_ATOMICS)
typedef int64_t SbAtomic64;
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>volatile const SbAtomic32*</code><br>
        <code>ptr</code></td>
    <td> </td>
  </tr>
</table>

### SbAtomicRelease_Load64

**Declaration**

```
static SbAtomic64 SbAtomicRelease_Load64(volatile const SbAtomic64* ptr);
#endif  // SB_HAS(64_BIT_ATOMICS)
// Pointer-sized atomic operations. Forwards to either 32-bit or 64-bit
// functions as appropriate.
#if SB_HAS(64_BIT_POINTERS)
typedef SbAtomic64 SbAtomicPtr;
#else
typedef SbAtomic32 SbAtomicPtr;
#endif
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>volatile const SbAtomic64*</code><br>
        <code>ptr</code></td>
    <td> </td>
  </tr>
</table>

### SbAtomicRelease_Store

**Declaration**

```
static void SbAtomicRelease_Store(volatile SbAtomic32* ptr, SbAtomic32 value);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>volatile SbAtomic32*</code><br>
        <code>ptr</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>SbAtomic32</code><br>
        <code>value</code></td>
    <td> </td>
  </tr>
</table>

### SbAtomicRelease_Store64

**Declaration**

```
static void SbAtomicRelease_Store64(volatile SbAtomic64* ptr, SbAtomic64 value);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>volatile SbAtomic64*</code><br>
        <code>ptr</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>SbAtomic64</code><br>
        <code>value</code></td>
    <td> </td>
  </tr>
</table>

### void

**Declaration**

```
static SB_C_FORCE_INLINE void
SbAtomicRelease_StorePtr(volatile SbAtomicPtr* ptr, SbAtomicPtr value) {
#if SB_HAS(64_BIT_POINTERS)
  SbAtomicRelease_Store64(ptr, value);
#else
  SbAtomicRelease_Store(ptr, value);
#endif
}
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbAtomicRelease_StorePtr(volatile SbAtomicPtr*</code><br>
        <code>ptr</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>SbAtomicPtr</code><br>
        <code>value</code></td>
    <td> </td>
  </tr>
</table>

