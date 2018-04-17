// Copyright 2015 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Module Overview: Starboard Atomic API
//
// Defines a set of atomic integer operations that can be used as lightweight
// synchronization or as building blocks for heavier synchronization primitives.
// Their use is very subtle and requires detailed understanding of the behavior
// of supported architectures, so their direct use is not recommended except
// when rigorously deemed absolutely necessary for performance reasons.

#ifndef STARBOARD_ATOMIC_H_
#define STARBOARD_ATOMIC_H_

#include "starboard/configuration.h"
#include "starboard/types.h"

#ifdef __cplusplus
extern "C" {
#endif

#if SB_API_VERSION >= SB_INTRODUCE_ATOMIC8_VERSION
typedef int8_t SbAtomic8;
#endif
typedef int32_t SbAtomic32;

// Atomically execute:
//      result = *ptr;
//      if (*ptr == old_value)
//        *ptr = new_value;
//      return result;
//
// I.e., replace "*ptr" with "new_value" if "*ptr" used to be "old_value".
// Always return the old value of "*ptr"
//
// This routine implies no memory barriers.
static SbAtomic32 SbAtomicNoBarrier_CompareAndSwap(volatile SbAtomic32* ptr,
                                                   SbAtomic32 old_value,
                                                   SbAtomic32 new_value);

// Atomically store new_value into *ptr, returning the previous value held in
// *ptr.  This routine implies no memory barriers.
static SbAtomic32 SbAtomicNoBarrier_Exchange(volatile SbAtomic32* ptr,
                                             SbAtomic32 new_value);

// Atomically increment *ptr by "increment".  Returns the new value of
// *ptr with the increment applied.  This routine implies no memory barriers.
static SbAtomic32 SbAtomicNoBarrier_Increment(volatile SbAtomic32* ptr,
                                              SbAtomic32 increment);

// Same as SbAtomicNoBarrier_Increment, but with a memory barrier.
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
static SbAtomic32 SbAtomicAcquire_CompareAndSwap(volatile SbAtomic32* ptr,
                                                 SbAtomic32 old_value,
                                                 SbAtomic32 new_value);
static SbAtomic32 SbAtomicRelease_CompareAndSwap(volatile SbAtomic32* ptr,
                                                 SbAtomic32 old_value,
                                                 SbAtomic32 new_value);

static void SbAtomicMemoryBarrier();
static void SbAtomicNoBarrier_Store(volatile SbAtomic32* ptr, SbAtomic32 value);
static void SbAtomicAcquire_Store(volatile SbAtomic32* ptr, SbAtomic32 value);
static void SbAtomicRelease_Store(volatile SbAtomic32* ptr, SbAtomic32 value);

static SbAtomic32 SbAtomicNoBarrier_Load(volatile const SbAtomic32* ptr);
static SbAtomic32 SbAtomicAcquire_Load(volatile const SbAtomic32* ptr);
static SbAtomic32 SbAtomicRelease_Load(volatile const SbAtomic32* ptr);

#if SB_API_VERSION >= SB_INTRODUCE_ATOMIC8_VERSION
// Overloaded functions for Atomic8.
static SbAtomic8 SbAtomicRelease_CompareAndSwap8(volatile SbAtomic8* ptr,
                                                 SbAtomic8 old_value,
                                                 SbAtomic8 new_value);
static void SbAtomicNoBarrier_Store8(volatile SbAtomic8* ptr, SbAtomic8 value);
static SbAtomic8 SbAtomicNoBarrier_Load8(volatile const SbAtomic8* ptr);
#endif

// 64-bit atomic operations (only available on 64-bit processors).
#if SB_HAS(64_BIT_ATOMICS)
typedef int64_t SbAtomic64;

static SbAtomic64 SbAtomicNoBarrier_CompareAndSwap64(volatile SbAtomic64* ptr,
                                                     SbAtomic64 old_value,
                                                     SbAtomic64 new_value);
static SbAtomic64 SbAtomicNoBarrier_Exchange64(volatile SbAtomic64* ptr,
                                               SbAtomic64 new_value);
static SbAtomic64 SbAtomicNoBarrier_Increment64(volatile SbAtomic64* ptr,
                                                SbAtomic64 increment);
static SbAtomic64 SbAtomicBarrier_Increment64(volatile SbAtomic64* ptr,
                                              SbAtomic64 increment);
static SbAtomic64 SbAtomicAcquire_CompareAndSwap64(volatile SbAtomic64* ptr,
                                                   SbAtomic64 old_value,
                                                   SbAtomic64 new_value);
static SbAtomic64 SbAtomicRelease_CompareAndSwap64(volatile SbAtomic64* ptr,
                                                   SbAtomic64 old_value,
                                                   SbAtomic64 new_value);
static void SbAtomicNoBarrier_Store64(volatile SbAtomic64* ptr,
                                      SbAtomic64 value);
static void SbAtomicAcquire_Store64(volatile SbAtomic64* ptr, SbAtomic64 value);
static void SbAtomicRelease_Store64(volatile SbAtomic64* ptr, SbAtomic64 value);
static SbAtomic64 SbAtomicNoBarrier_Load64(volatile const SbAtomic64* ptr);
static SbAtomic64 SbAtomicAcquire_Load64(volatile const SbAtomic64* ptr);
static SbAtomic64 SbAtomicRelease_Load64(volatile const SbAtomic64* ptr);
#endif  // SB_HAS(64_BIT_ATOMICS)

// Pointer-sized atomic operations. Forwards to either 32-bit or 64-bit
// functions as appropriate.
#if SB_HAS(64_BIT_POINTERS)
typedef SbAtomic64 SbAtomicPtr;
#else
typedef SbAtomic32 SbAtomicPtr;
#endif

static SB_C_FORCE_INLINE SbAtomicPtr
SbAtomicNoBarrier_CompareAndSwapPtr(volatile SbAtomicPtr* ptr,
                                    SbAtomicPtr old_value,
                                    SbAtomicPtr new_value) {
#if SB_HAS(64_BIT_POINTERS)
  return SbAtomicNoBarrier_CompareAndSwap64(ptr, old_value, new_value);
#else
  return SbAtomicNoBarrier_CompareAndSwap(ptr, old_value, new_value);
#endif
}

static SB_C_FORCE_INLINE SbAtomicPtr
SbAtomicNoBarrier_ExchangePtr(volatile SbAtomicPtr* ptr,
                              SbAtomicPtr new_value) {
#if SB_HAS(64_BIT_POINTERS)
  return SbAtomicNoBarrier_Exchange64(ptr, new_value);
#else
  return SbAtomicNoBarrier_Exchange(ptr, new_value);
#endif
}

static SB_C_FORCE_INLINE SbAtomicPtr
SbAtomicNoBarrier_IncrementPtr(volatile SbAtomicPtr* ptr,
                               SbAtomicPtr increment) {
#if SB_HAS(64_BIT_POINTERS)
  return SbAtomicNoBarrier_Increment64(ptr, increment);
#else
  return SbAtomicNoBarrier_Increment(ptr, increment);
#endif
}

static SB_C_FORCE_INLINE SbAtomicPtr
SbAtomicBarrier_IncrementPtr(volatile SbAtomicPtr* ptr, SbAtomicPtr increment) {
#if SB_HAS(64_BIT_POINTERS)
  return SbAtomicBarrier_Increment64(ptr, increment);
#else
  return SbAtomicBarrier_Increment(ptr, increment);
#endif
}

static SB_C_FORCE_INLINE SbAtomicPtr
SbAtomicAcquire_CompareAndSwapPtr(volatile SbAtomicPtr* ptr,
                                  SbAtomicPtr old_value,
                                  SbAtomicPtr new_value) {
#if SB_HAS(64_BIT_POINTERS)
  return SbAtomicAcquire_CompareAndSwap64(ptr, old_value, new_value);
#else
  return SbAtomicAcquire_CompareAndSwap(ptr, old_value, new_value);
#endif
}

static SB_C_FORCE_INLINE SbAtomicPtr
SbAtomicRelease_CompareAndSwapPtr(volatile SbAtomicPtr* ptr,
                                  SbAtomicPtr old_value,
                                  SbAtomicPtr new_value) {
#if SB_HAS(64_BIT_POINTERS)
  return SbAtomicRelease_CompareAndSwap64(ptr, old_value, new_value);
#else
  return SbAtomicRelease_CompareAndSwap(ptr, old_value, new_value);
#endif
}

static SB_C_FORCE_INLINE void
SbAtomicNoBarrier_StorePtr(volatile SbAtomicPtr* ptr, SbAtomicPtr value) {
#if SB_HAS(64_BIT_POINTERS)
  SbAtomicNoBarrier_Store64(ptr, value);
#else
  SbAtomicNoBarrier_Store(ptr, value);
#endif
}

static SB_C_FORCE_INLINE void
SbAtomicAcquire_StorePtr(volatile SbAtomicPtr* ptr, SbAtomicPtr value) {
#if SB_HAS(64_BIT_POINTERS)
  SbAtomicAcquire_Store64(ptr, value);
#else
  SbAtomicAcquire_Store(ptr, value);
#endif
}

static SB_C_FORCE_INLINE void
SbAtomicRelease_StorePtr(volatile SbAtomicPtr* ptr, SbAtomicPtr value) {
#if SB_HAS(64_BIT_POINTERS)
  SbAtomicRelease_Store64(ptr, value);
#else
  SbAtomicRelease_Store(ptr, value);
#endif
}

static SB_C_FORCE_INLINE SbAtomicPtr
SbAtomicNoBarrier_LoadPtr(volatile const SbAtomicPtr* ptr) {
#if SB_HAS(64_BIT_POINTERS)
  return SbAtomicNoBarrier_Load64(ptr);
#else
  return SbAtomicNoBarrier_Load(ptr);
#endif
}

static SB_C_FORCE_INLINE SbAtomicPtr
SbAtomicAcquire_LoadPtr(volatile const SbAtomicPtr* ptr) {
#if SB_HAS(64_BIT_POINTERS)
  return SbAtomicAcquire_Load64(ptr);
#else
  return SbAtomicAcquire_Load(ptr);
#endif
}

static SB_C_FORCE_INLINE SbAtomicPtr
SbAtomicRelease_LoadPtr(volatile const SbAtomicPtr* ptr) {
#if SB_HAS(64_BIT_POINTERS)
  return SbAtomicRelease_Load64(ptr);
#else
  return SbAtomicRelease_Load(ptr);
#endif
}

#ifdef __cplusplus
}  // extern "C"
#endif

// C++ Can choose the right function based on overloaded arguments. This
// provides overloaded versions that will choose the right function version
// based on type for C++ callers.

#ifdef __cplusplus
namespace starboard {
namespace atomic {

#if SB_API_VERSION >= SB_INTRODUCE_ATOMIC8_VERSION
inline SbAtomic8 Release_CompareAndSwap(volatile SbAtomic8* ptr,
                                        SbAtomic8 old_value,
                                        SbAtomic8 new_value) {
  return SbAtomicRelease_CompareAndSwap8(ptr, old_value, new_value);
}

inline void NoBarrier_Store(volatile SbAtomic8* ptr, SbAtomic8 value) {
  SbAtomicNoBarrier_Store8(ptr, value);
}

inline SbAtomic8 NoBarrier_Load(volatile const SbAtomic8* ptr) {
  return SbAtomicNoBarrier_Load8(ptr);
}
#endif

inline SbAtomic32 NoBarrier_CompareAndSwap(volatile SbAtomic32* ptr,
                                           SbAtomic32 old_value,
                                           SbAtomic32 new_value) {
  return SbAtomicNoBarrier_CompareAndSwap(ptr, old_value, new_value);
}

inline SbAtomic32 NoBarrier_Exchange(volatile SbAtomic32* ptr,
                                     SbAtomic32 new_value) {
  return SbAtomicNoBarrier_Exchange(ptr, new_value);
}

inline SbAtomic32 NoBarrier_Increment(volatile SbAtomic32* ptr,
                                      SbAtomic32 increment) {
  return SbAtomicNoBarrier_Increment(ptr, increment);
}

inline SbAtomic32 Barrier_Increment(volatile SbAtomic32* ptr,
                                    SbAtomic32 increment) {
  return SbAtomicBarrier_Increment(ptr, increment);
}

inline SbAtomic32 Acquire_CompareAndSwap(volatile SbAtomic32* ptr,
                                         SbAtomic32 old_value,
                                         SbAtomic32 new_value) {
  return SbAtomicAcquire_CompareAndSwap(ptr, old_value, new_value);
}

inline SbAtomic32 Release_CompareAndSwap(volatile SbAtomic32* ptr,
                                         SbAtomic32 old_value,
                                         SbAtomic32 new_value) {
  return SbAtomicRelease_CompareAndSwap(ptr, old_value, new_value);
}

inline void NoBarrier_Store(volatile SbAtomic32* ptr, SbAtomic32 value) {
  SbAtomicNoBarrier_Store(ptr, value);
}

inline void Acquire_Store(volatile SbAtomic32* ptr, SbAtomic32 value) {
  SbAtomicAcquire_Store(ptr, value);
}

inline void Release_Store(volatile SbAtomic32* ptr, SbAtomic32 value) {
  SbAtomicRelease_Store(ptr, value);
}

inline SbAtomic32 NoBarrier_Load(volatile const SbAtomic32* ptr) {
  return SbAtomicNoBarrier_Load(ptr);
}

inline SbAtomic32 Acquire_Load(volatile const SbAtomic32* ptr) {
  return SbAtomicAcquire_Load(ptr);
}

inline SbAtomic32 Release_Load(volatile const SbAtomic32* ptr) {
  return SbAtomicRelease_Load(ptr);
}

#if SB_HAS(64_BIT_ATOMICS)
inline SbAtomic64 NoBarrier_CompareAndSwap(volatile SbAtomic64* ptr,
                                           SbAtomic64 old_value,
                                           SbAtomic64 new_value) {
  return SbAtomicNoBarrier_CompareAndSwap64(ptr, old_value, new_value);
}

inline SbAtomic64 NoBarrier_Exchange(volatile SbAtomic64* ptr,
                                     SbAtomic64 new_value) {
  return SbAtomicNoBarrier_Exchange64(ptr, new_value);
}

inline SbAtomic64 NoBarrier_Increment(volatile SbAtomic64* ptr,
                                      SbAtomic64 increment) {
  return SbAtomicNoBarrier_Increment64(ptr, increment);
}

inline SbAtomic64 Barrier_Increment(volatile SbAtomic64* ptr,
                                    SbAtomic64 increment) {
  return SbAtomicBarrier_Increment64(ptr, increment);
}

inline SbAtomic64 Acquire_CompareAndSwap(volatile SbAtomic64* ptr,
                                         SbAtomic64 old_value,
                                         SbAtomic64 new_value) {
  return SbAtomicAcquire_CompareAndSwap64(ptr, old_value, new_value);
}

inline SbAtomic64 Release_CompareAndSwap(volatile SbAtomic64* ptr,
                                         SbAtomic64 old_value,
                                         SbAtomic64 new_value) {
  return SbAtomicRelease_CompareAndSwap64(ptr, old_value, new_value);
}

inline void NoBarrier_Store(volatile SbAtomic64* ptr, SbAtomic64 value) {
  SbAtomicNoBarrier_Store64(ptr, value);
}

inline void Acquire_Store(volatile SbAtomic64* ptr, SbAtomic64 value) {
  SbAtomicAcquire_Store64(ptr, value);
}

inline void Release_Store(volatile SbAtomic64* ptr, SbAtomic64 value) {
  SbAtomicRelease_Store64(ptr, value);
}

inline SbAtomic64 NoBarrier_Load(volatile const SbAtomic64* ptr) {
  return SbAtomicNoBarrier_Load64(ptr);
}

inline SbAtomic64 Acquire_Load(volatile const SbAtomic64* ptr) {
  return SbAtomicAcquire_Load64(ptr);
}

inline SbAtomic64 Release_Load(volatile const SbAtomic64* ptr) {
  return SbAtomicRelease_Load64(ptr);
}
#endif  // SB_HAS(64_BIT_ATOMICS)

}  // namespace atomic
}  // namespace starboard

#endif

// Include the platform definitions of these functions, which should be defined
// as inlined. This macro is defined on the command line by gyp.
#include STARBOARD_ATOMIC_INCLUDE

#ifdef __cplusplus

#include "starboard/mutex.h"

namespace starboard {

// Provides atomic types like integer and float. Some types like atomic_int32_t
// are likely to be hardware accelerated for your platform.
//
// Never use the parent types like atomic_base<T>, atomic_number<T> or
// atomic_integral<T> and instead use the fully qualified classes like
// atomic_int32_t, atomic_pointer<T*>, etc.
//
// Note on template instantiation, avoid using the parent type and instead
// use the fully qualified type.
// BAD:
//  template<typename T>
//  void Foo(const atomic_base<T>& value);
// GOOD:
//  template<typename atomic_t>
//  void Foo(const atomic_t& vlaue);

// Atomic Pointer class. Instantiate as atomic_pointer<void*>
// for void* pointer types.
template <typename T>
class atomic_pointer;

// Atomic bool class.
class atomic_bool;

// Atomic int32 class
class atomic_int32_t;

// Atomic int64 class.
class atomic_int64_t;

// Atomic float class.
class atomic_float;

// Atomic double class.
class atomic_double;

///////////////////////////////////////////////////////////////////////////////
// Class hiearchy.
///////////////////////////////////////////////////////////////////////////////

// Base functionality for atomic types. Defines exchange(), load(),
// store(), compare_exhange_weak(), compare_exchange_strong()
template <typename T>
class atomic_base;

// Subtype of atomic_base<T> for numbers likes float and integer types but not
// bool.  Adds fetch_add() and fetch_sub().
template <typename T>
class atomic_number;

// Subtype of atomic_number<T> for integer types like int32 and int64. Adds
// increment and decrement.
template <typename T>
class atomic_integral;

///////////////////////////////////////////////////////////////////////////////
// Implementation.
///////////////////////////////////////////////////////////////////////////////

// Similar to C++11 std::atomic<T>.
// atomic_base<T> may be instantiated with any TriviallyCopyable type T.
// atomic_base<T> is neither copyable nor movable.
template <typename T>
class atomic_base {
 public:
  typedef T ValueType;

  // C++11 forbids a copy constructor for std::atomic<T>, it also forbids
  // a move operation.
  atomic_base() : value_() {}
  explicit atomic_base(T v) : value_(v) {}

  // Checks whether the atomic operations on all objects of this type
  // are lock-free.
  // Returns true if the atomic operations on the objects of this type
  // are lock-free, false otherwise.
  //
  // All atomic types may be implemented using mutexes or other locking
  // operations, rather than using the lock-free atomic CPU instructions.
  // atomic types are also allowed to be sometimes lock-free, e.g. if only
  // aligned memory accesses are naturally atomic on a given architecture,
  // misaligned objects of the same type have to use locks.
  //
  // See also std::atomic<T>::is_lock_free().
  bool is_lock_free() const { return false; }
  bool is_lock_free() const volatile { return false; }

  // Atomically replaces the value of the atomic object and returns the value
  // held previously.
  // See also std::atomic<T>::exchange().
  T exchange(T new_val) {
    T old_value;
    {
      starboard::ScopedLock lock(mutex_);
      old_value = value_;
      value_ = new_val;
    }
    return old_value;
  }

  // Atomically obtains the value of the atomic object.
  // See also std::atomic<T>::load().
  T load() const {
    starboard::ScopedLock lock(mutex_);
    return value_;
  }

  // Stores the value. See std::atomic<T>::store(...)
  void store(T val) {
    starboard::ScopedLock lock(mutex_);
    value_ = val;
  }

  // compare_exchange_strong(...) sets the new value if and only if
  // *expected_value matches what is stored internally.
  // If this succeeds then true is returned and *expected_value == new_value.
  // Otherwise If there is a mismatch then false is returned and
  // *expected_value is set to the internal value.
  // Inputs:
  //  new_value: Attempt to set the value to this new value.
  //  expected_value: A test condition for success. If the actual value
  //    matches the expected_value then the swap will succeed.
  //
  // See also std::atomic<T>::compare_exchange_strong(...).
  bool compare_exchange_strong(T* expected_value, T new_value) {
    // Save original value so that its local. This hints to the compiler
    // that test_val doesn't have aliasing issues and should result in
    // more optimal code inside of the lock.
    const T test_val = *expected_value;
    starboard::ScopedLock lock(mutex_);
    if (test_val == value_) {
      value_ = new_value;
      return true;
    } else {
      *expected_value = value_;
      return false;
    }
  }

  // Weak version of this function is documented to be faster, but has allows
  // weaker memory ordering and therefore will sometimes have a false negative:
  // The value compared will actually be equal but the return value from this
  // function indicates otherwise.
  // By default, the function delegates to compare_exchange_strong(...).
  //
  // See also std::atomic<T>::compare_exchange_weak(...).
  bool compare_exchange_weak(T* expected_value, T new_value) {
    return compare_exchange_strong(expected_value, new_value);
  }

 protected:
  T value_;
  starboard::Mutex mutex_;
};

// A subclass of atomic_base<T> that adds fetch_add(...) and fetch_sub(...).
template <typename T>
class atomic_number : public atomic_base<T> {
 public:
  typedef atomic_base<T> Super;
  typedef T ValueType;

  atomic_number() : Super() {}
  explicit atomic_number(T v) : Super(v) {}

  // Returns the previous value before the input value was added.
  // See also std::atomic<T>::fetch_add(...).
  T fetch_add(T val) {
    T old_val;
    {
      starboard::ScopedLock lock(this->mutex_);
      old_val = this->value_;
      this->value_ += val;
    }
    return old_val;
  }

  // Returns the value before the operation was applied.
  // See also std::atomic<T>::fetch_sub(...).
  T fetch_sub(T val) {
    T old_val;
    {
      starboard::ScopedLock lock(this->mutex_);
      old_val = this->value_;
      this->value_ -= val;
    }
    return old_val;
  }
};

// A subclass to classify Atomic Integers. Adds increment and decrement
// functions.
template <typename T>
class atomic_integral : public atomic_number<T> {
 public:
  typedef atomic_number<T> Super;

  atomic_integral() : Super() {}
  explicit atomic_integral(T v) : Super(v) {}

  T increment() { return this->fetch_add(T(1)); }
  T decrement() { return this->fetch_sub(T(1)); }
};

// atomic_pointer class.
template <typename T>
class atomic_pointer : public atomic_base<T> {
 public:
  typedef atomic_base<T> Super;
  atomic_pointer() : Super() {}
  explicit atomic_pointer(T initial_val) : Super(initial_val) {}
};

// Simple atomic bool class.
class atomic_bool {
 public:
  typedef bool ValueType;

  // C++11 forbids a copy constructor for std::atomic<T>, it also forbids
  // a move operation.
  atomic_bool() : value_(false) {}
  explicit atomic_bool(bool v) : value_(v) {}

  bool is_lock_free() const { return true; }
  bool is_lock_free() const volatile { return true; }

  bool exchange(bool new_val) {
    return SbAtomicNoBarrier_Exchange(volatile_ptr(), new_val);
  }

  bool load() const { return SbAtomicAcquire_Load(volatile_const_ptr()) != 0; }

  void store(bool val) { SbAtomicRelease_Store(volatile_ptr(), val); }

 private:
  volatile int32_t* volatile_ptr() { return &value_; }
  volatile const int32_t* volatile_const_ptr() const { return &value_; }
  int32_t value_;
};

// Lockfree atomic int class.
class atomic_int32_t {
 public:
  typedef int32_t ValueType;
  atomic_int32_t() : value_(0) {}
  explicit atomic_int32_t(SbAtomic32 value) : value_(value) {}

  bool is_lock_free() const { return true; }
  bool is_lock_free() const volatile { return true; }

  int32_t increment() { return fetch_add(1); }
  int32_t decrement() { return fetch_add(-1); }

  int32_t fetch_add(int32_t val) {
    // fetch_add is a post-increment operation, while SbAtomicBarrier_Increment
    // is a pre-increment operation. Therefore subtract the value to match
    // the expected interface.
    return SbAtomicBarrier_Increment(volatile_ptr(), val) - val;
  }

  int32_t fetch_sub(int32_t val) { return fetch_add(-val); }

  // Atomically replaces the value of the atomic object
  // and returns the value held previously.
  // See also std::atomic<T>::exchange().
  int32_t exchange(int32_t new_val) {
    return SbAtomicNoBarrier_Exchange(volatile_ptr(), new_val);
  }

  // Atomically obtains the value of the atomic object.
  // See also std::atomic<T>::load().
  int32_t load() const { return SbAtomicAcquire_Load(volatile_const_ptr()); }

  // Stores the value. See std::atomic<T>::store(...)
  void store(int32_t val) { SbAtomicRelease_Store(volatile_ptr(), val); }

  bool compare_exchange_strong(int32_t* expected_value, int32_t new_value) {
    int32_t prev_value = *expected_value;
    SbAtomicMemoryBarrier();
    int32_t value_written =
        SbAtomicRelease_CompareAndSwap(volatile_ptr(), prev_value, new_value);
    const bool write_ok = (prev_value == value_written);
    if (!write_ok) {
      *expected_value = value_written;
    }
    return write_ok;
  }

  // Weak version of this function is documented to be faster, but has allows
  // weaker memory ordering and therefore will sometimes have a false negative:
  // The value compared will actually be equal but the return value from this
  // function indicates otherwise.
  // By default, the function delegates to compare_exchange_strong(...).
  //
  // See also std::atomic<T>::compare_exchange_weak(...).
  bool compare_exchange_weak(int32_t* expected_value, int32_t new_value) {
    return compare_exchange_strong(expected_value, new_value);
  }

 private:
  volatile int32_t* volatile_ptr() { return &value_; }
  volatile const int32_t* volatile_const_ptr() const { return &value_; }
  int32_t value_;
};

// Simple atomic int class. This could be optimized for speed using
// compiler intrinsics for concurrent integer modification.
class atomic_int64_t : public atomic_integral<int64_t> {
 public:
  typedef atomic_integral<int64_t> Super;
  atomic_int64_t() : Super() {}
  explicit atomic_int64_t(int64_t initial_val) : Super(initial_val) {}
};

class atomic_float : public atomic_number<float> {
 public:
  typedef atomic_number<float> Super;
  atomic_float() : Super() {}
  explicit atomic_float(float initial_val) : Super(initial_val) {}
};

class atomic_double : public atomic_number<double> {
 public:
  typedef atomic_number<double> Super;
  atomic_double() : Super() {}
  explicit atomic_double(double initial_val) : Super(initial_val) {}
};

}  // namespace starboard

#endif  // __cplusplus

#endif  // STARBOARD_ATOMIC_H_
