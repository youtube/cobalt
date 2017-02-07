/*
 * Copyright 2016 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef NB_ATOMIC_H_
#define NB_ATOMIC_H_

#include "starboard/atomic.h"
#include "starboard/mutex.h"
#include "starboard/types.h"

namespace nb {

// Provides atomic types like integer and float. Some types like atomic_int32_t
// are likely to be hardware accelerated for your platform.
//
// Never use the parent types like atomic<T>, atomic_number<T> or
// atomic_integral<T> and instead use the fully qualified classes like
// atomic_int32_t, atomic_pointer<T*>, etc.
//
// Note on template instantiation, avoid using the parent type and instead
// use the fully qualified type.
// BAD:
//  template<typename T>
//  void Foo(const atomic<T>& value);
// GOOD:
//  template<typename atomic_t>
//  void Foo(const atomic_t& vlaue);

// Atomic Pointer class. Instantiate as atomic_pointer<void*>
// for void* pointer types.
template<typename T>
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
class atomic;

// Subtype of atomic<T> for numbers likes float and integer types but not bool.
// Adds fetch_add() and fetch_sub().
template <typename T>
class atomic_number;

// Subtype of atomic_number<T> for integer types like int32 and int64. Adds
// increment and decrement.
template <typename T>
class atomic_integral;

///////////////////////////////////////////////////////////////////////////////
// Implimentation.
///////////////////////////////////////////////////////////////////////////////

// Similar to C++11 std::atomic<T>.
// atomic<T> may be instantiated with any TriviallyCopyable type T.
// atomic<T> is neither copyable nor movable.
template <typename T>
class atomic {
 public:
  typedef T ValueType;

  // C++11 forbids a copy constructor for std::atomic<T>, it also forbids
  // a move operation.
  atomic() : value_() {}
  explicit atomic(T v) : value_(v) {}

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

  // Atomically replaces the value of the atomic object
  // and returns the value held previously.
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

// A subclass of atomic<T> that adds fetch_add(...) and fetch_sub(...).
template <typename T>
class atomic_number : public atomic<T> {
 public:
  typedef atomic<T> Super;
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
class atomic_pointer : public atomic<T> {
 public:
  typedef atomic<T> Super;
  atomic_pointer() : Super() {}
  explicit atomic_pointer(T initial_val) : Super(initial_val) {}
};

// Simple atomic bool class. This could be optimized for speed using
// compiler intrinsics for concurrent integer modification.
class atomic_bool : public atomic<bool> {
 public:
  typedef atomic<bool> Super;
  atomic_bool() : Super() {}
  explicit atomic_bool(bool initial_val) : Super(initial_val) {}
};

// Lockfree atomic int class.
class atomic_int32_t {
 public:
  typedef int32_t ValueType;
  atomic_int32_t() : value_(0) {}
  atomic_int32_t(SbAtomic32 value) : value_(value) {}

  bool is_lock_free() const { return true; }
  bool is_lock_free() const volatile { return true; }

  int32_t increment() {
    return fetch_add(1);
  }
  int32_t decrement() {
    return fetch_add(-1);
  }

  int32_t fetch_add(int32_t val) {
    // fetch_add is a post-increment operation, while SbAtomicBarrier_Increment
    // is a pre-increment operation. Therefore subtract the value to match
    // the expected interface.
    return SbAtomicBarrier_Increment(volatile_ptr(), val) - val;
  }

 int32_t fetch_sub(int32_t val) {
   return fetch_add(-val);
  }

  // Atomically replaces the value of the atomic object
  // and returns the value held previously.
  // See also std::atomic<T>::exchange().
  int32_t exchange(int32_t new_val) {
    return SbAtomicNoBarrier_Exchange(volatile_ptr(), new_val);
  }

  // Atomically obtains the value of the atomic object.
  // See also std::atomic<T>::load().
  int32_t load() const {
    return SbAtomicAcquire_Load(volatile_const_ptr());
  }

  // Stores the value. See std::atomic<T>::store(...)
  void store(int32_t val) {
    SbAtomicRelease_Store(volatile_ptr(), val);
  }

  bool compare_exchange_strong(int32_t* expected_value, int32_t new_value) {
    int32_t prev_value = *expected_value;
    SbAtomicMemoryBarrier();
    int32_t value_written = SbAtomicRelease_CompareAndSwap(volatile_ptr(),
                                                           prev_value,
                                                           new_value);
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

}  // namespace nb

#endif  // NB_ATOMIC_H_
