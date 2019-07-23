/*
 * Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef NB_CONCURRENT_PTR_H_
#define NB_CONCURRENT_PTR_H_

#include <algorithm>
#include <atomic>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "starboard/atomic.h"
#include "starboard/common/log.h"
#include "starboard/common/mutex.h"
#include "starboard/memory.h"
#include "starboard/thread.h"
#include "starboard/types.h"

namespace nb {
namespace detail {
class AtomicPointerBase;

template <typename T>
class AtomicPointer;

template <typename T>
class Access;
}  // namespace detail

// ConcurrentPtr is similar to a scoped_ptr<> but with additional thread safe
//  guarantees on the lifespan of the owned pointer. Threads can get access
//  to the owned pointer using class ConcurrentPtr<T>::Access, which will lock
//  the lifetime of the pointer for the duration that ConcurrentPtr<T>::Access
//  object will remain in scope. Operations on the object pointed to by the
//  owned pointer has no additional thread safety guarantees.
//
//
// ConcurrentPtr<T>::access_ptr(...) will take in an arbitrary id value and
//  will hash this id value to select one of the buckets. Good id's to pass in
//  include input from SbThreadGetId() or random numbers (std::rand() is known
//  to use internal locks on some implementations!).
//
// Performance:
//  To increase performance, the lifespan of ConcurrentPtr<T>::Access instance
//  should be as short as possible.
//
//  The number of buckets has a large effect on performance. More buckets will
//  result in lower contention.
//
//  A hashing function with good distribution will produce less contention.
//
// Example:
//  class MyClass { void Run(); };
//  ConcurrentPtr<MyClass> shared_concurrent_ptr_(new MyClass);
//
//  // From all other threads.
//  ConcurrentPtr<MyClass>::Access access_ptr =
//      shared_concurrent_ptr_.access_ptr(SbThreadGetId());
//  // access_ptr now either holds a valid object pointer or either nullptr.
//  if (access_ptr) {
//    access_ptr->Run();
//  }
template <typename T, typename KeyT = int64_t, typename HashT = std::hash<KeyT>>
class ConcurrentPtr {
 public:
  ConcurrentPtr(T* ptr, size_t number_locks = 31) : ptr_(NULL) {
    internal_construct(ptr, number_locks);
  }
  ~ConcurrentPtr() { internal_destruct(); }

  // Used to access the underlying pointer to perform operations. The lifetime
  //  of the accessed pointer is guaranteed to be alive for the duration of the
  //  lifetime of this access object.
  //
  // Example
  //  ConcurrentPtr<MyClass>::Access access_ptr =
  //      shared_concurrent_ptr_.access_ptr(SbThreadGetId());
  //  if (access_ptr) {
  //    access_ptr->Run();
  //  }
  using Access = nb::detail::Access<T>;

  // Provides read access to the underlying pointer in a thread safe way.
  inline Access access_ptr(const KeyT& seed) {
    const size_t index = hasher_(seed) % table_.size();
    AtomicPointer* atomic_ptr = table_[index];
    return atomic_ptr->access_ptr();
  }

  void reset(T* value) { delete SetAllThenSwap(value); }
  T* release() { return SetAllThenSwap(nullptr); }
  T* swap(T* new_value) { return SetAllThenSwap(new_value); }

 private:
  using Mutex = starboard::Mutex;
  using ScopedLock = starboard::ScopedLock;
  using AtomicPointer = nb::detail::AtomicPointer<T>;

  // Change all buckets to the new pointer. The old pointer is returned.
  T* SetAllThenSwap(T* new_ptr) {
    ScopedLock write_lock(pointer_write_mutex_);
    for (auto it = table_.begin(); it != table_.end(); ++it) {
      AtomicPointer* atomic_ptr = *it;
      atomic_ptr->swap(new_ptr);
    }
    T* old_ptr = ptr_;
    ptr_ = new_ptr;
    return old_ptr;
  }

  void internal_construct(T* ptr, size_t number_locks) {
    table_.resize(number_locks);
    for (auto it = table_.begin(); it != table_.end(); ++it) {
      *it = new AtomicPointer;
    }
    reset(ptr);
  }

  void internal_destruct() {
    reset(nullptr);
    for (auto it = table_.begin(); it != table_.end(); ++it) {
      delete *it;
    }
    table_.clear();
  }
  HashT hasher_;
  Mutex pointer_write_mutex_;
  std::vector<AtomicPointer*> table_;
  T* ptr_;
};

/////////////////////////// Implementation Details ////////////////////////////
namespace detail {

// Access is a smart pointer type which holds a lock to the underlying pointer
// returned by ConcurrentPtr and AtomicPointer.
template <typename T>
class Access {
 public:
  Access() = delete;
  // It's assumed that ref_count is incremented before being passed
  // to this constructor.
  inline Access(T* ptr, starboard::atomic_int32_t* ref_count)
      : ref_count_(ref_count), ptr_(ptr) {}

  Access(const Access& other) = delete;
  // Allow move construction.
  Access(Access&& other) = default;

  inline ~Access() { release(); }

  inline operator bool() const { return valid(); }
  inline operator T*() const { return get(); }
  inline T* operator->() { return get(); }
  inline T* get() const { return ptr_; }
  inline bool valid() const { return !!get(); }
  inline void release() { internal_release(); }

 private:
  inline void internal_release() {
    if (ref_count_) {
      ref_count_->decrement();
      ref_count_ = nullptr;
    }
    ptr_ = nullptr;
  }
  starboard::atomic_int32_t* ref_count_;
  T* ptr_;
};

// AtomicPointer allows read access to a pointer through access_ptr()
// and a form of atomic swap.
template <typename T>
class AtomicPointer {
 public:
  // Customer new/delete operators align AtomicPointers to cache
  // lines for improved performance.
  static void* operator new(std::size_t count) {
    const int kCacheLineSize = 64;
    return SbMemoryAllocateAligned(kCacheLineSize, count);
  }
  static void operator delete(void* ptr) { SbMemoryDeallocateAligned(ptr); }

  AtomicPointer() : ptr_(nullptr), counter_(0) {}
  ~AtomicPointer() { delete get(); }
  inline T* swap(T* new_ptr) {
    AcquireWriteLock();
    T* old_ptr = get();
    ptr_.store(new_ptr);
    WaitForReadersToDrain();
    ReleaseWriteLock();
    return old_ptr;
  }

  inline void reset(T* new_ptr) { delete swap(new_ptr); }
  inline T* get() { return ptr_.load(); }
  inline Access<T> access_ptr() {
    counter_.increment();
    return Access<T>(ptr_.load(), &counter_);
  }

 private:
  inline void AcquireWriteLock() {
    write_mutex_.Acquire();
    counter_.increment();
  }
  inline void WaitForReadersToDrain() {
    int32_t expected_value = 1;
    while (!counter_.compare_exchange_weak(&expected_value, 0)) {
      SbThreadYield();
      expected_value = 1;
    }
  }
  inline void ReleaseWriteLock() { write_mutex_.Release(); }

  starboard::Mutex write_mutex_;
  starboard::atomic_int32_t counter_;
  starboard::atomic_pointer<T*> ptr_;
};
}  // namespace detail
}  // namespace nb

#endif  // NB_CONCURRENT_PTR_H_
