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

#ifndef NB_STD_ALLOCATOR_H_
#define NB_STD_ALLOCATOR_H_

#include <memory>

#include "nb/allocator.h"
#include "starboard/configuration.h"
#include "starboard/types.h"

namespace nb {

// A standard container compatible allocator that delegates allocations to a
// custom allocator. This allows standard containers like vector<> and map<> to
// use custom allocator schemes.
//
// AllocatorT:
//   This is the backing allocator that implements Allocate(...) and
//   Deallocate(...).
//
// Example:
// struct AllocatorImpl {
//   static void* Allocate(size_t n) {
//     return SbMemoryAllocate(n);
//   }
//   // Second argument can be used for accounting, but is otherwise optional.
//   static void Deallocate(void* ptr, size_t n) {
//     SbMemoryDeallocate(ptr);
//   }
// };
//
// typedef std::vector<int, StdAllocator<T, AllocatorImpl> > MyVector;
// MyVector vector;
// ...
template <typename T, typename AllocatorT>
class StdAllocator : public std::allocator<T> {
 public:
  typedef typename std::allocator<T>::pointer pointer;
  typedef typename std::allocator<T>::const_pointer const_pointer;
  typedef typename std::allocator<T>::reference reference;
  typedef typename std::allocator<T>::const_reference const_reference;
  typedef typename std::allocator<T>::size_type size_type;
  typedef typename std::allocator<T>::value_type value_type;
  typedef typename std::allocator<T>::difference_type difference_type;

  StdAllocator() {}

  // Constructor used for rebinding
  template <typename U, typename V>
  StdAllocator(const StdAllocator<U, V>&) {}

  pointer allocate(size_type n,
                   std::allocator<void>::const_pointer hint = NULL) {
    SB_UNREFERENCED_PARAMETER(hint);
    void* ptr = AllocatorT::Allocate(n * sizeof(value_type));
    return static_cast<pointer>(ptr);
  }

  void deallocate(pointer ptr, size_type n) {
    AllocatorT::Deallocate(ptr, n * sizeof(value_type));
  }
  template <typename U>
  struct rebind {
    typedef StdAllocator<U, AllocatorT> other;
  };
};

// A standard container compatible allocator that delegates allocations to a
// custom allocator via a shared pointer. This differs from StdAllocator since
// StdAllocator binds to static functions. This important difference allows
// StdDynamicAllocator to keep memory accounting on a per-instance basis, but
// otherwise is a tad slower, harder to instantiate and produces less readable
// code.
//
// When in doubt, use StdAllocator over StdDynamicAllocator.
//
// Passed in nb::Allocator:
//  Even though nb::Allocator has many functions for alignment, only the two
//  are used within StdDynamicAllocator:
//    void* nb::Allocator::Allocate() -and-
//    void nb::FreeWithSize(void* ptr, size_t optional_size)
//
// Example of Allocator Definition:
//  class MyAllocator : public SimpleAllocator {
//   public:
//    void* Allocate(size_t size) override {
//      return SbMemoryAllocate(size);
//    }
//
//    // Second argument can be used for accounting, but is otherwise optional.
//    void FreeWithSize(void* ptr, size_t optional_size) override {
//      SbMemoryDeallocate(ptr);
//    }
//
//    // ... other functions
//  };
//
// Example of std::vector<int>:
//  typedef StdDynamicAllocator<int> IntAllocator;
//  typedef std::vector<int, IntAllocator> IntVector;
//
//  MyAllocator my_allocator;
//  // Note that IntVector is not default constructible!
//  IntVector int_vector(IntAllocator(&my_allocator));
//
// Example of std::map<int, int>:
//  // Note that maps require std::less() instance to be passed in whenever
//  // a custom allocator is passed in.
//  typedef std::map<int, int, std::less<int>, MyAllocator> IntMap;
//  IntMap int_map(std::less<int>(), /* std::less<int> required pre-C++11 */
//                 IntAllocator(&my_allocator));
template <typename T>
class StdDynamicAllocator : public std::allocator<T> {
 public:
  typedef typename std::allocator<T>::pointer pointer;
  typedef typename std::allocator<T>::const_pointer const_pointer;
  typedef typename std::allocator<T>::reference reference;
  typedef typename std::allocator<T>::const_reference const_reference;
  typedef typename std::allocator<T>::size_type size_type;
  typedef typename std::allocator<T>::value_type value_type;
  typedef typename std::allocator<T>::difference_type difference_type;

  explicit StdDynamicAllocator(Allocator* allocator) : allocator_(allocator) {}
  StdDynamicAllocator(StdDynamicAllocator& std_allocator)
      : allocator_(std_allocator.allocator_) {}

  // Constructor used for rebinding
  template <typename U>
  StdDynamicAllocator(const StdDynamicAllocator<U>& x)
      : allocator_(x.allocator()) {}

  pointer allocate(size_type n,
                   std::allocator<void>::const_pointer hint = NULL) {
    SB_UNREFERENCED_PARAMETER(hint);
    void* ptr = allocator_->Allocate(n * sizeof(value_type));
    return static_cast<pointer>(ptr);
  }

  void deallocate(pointer ptr, size_type n) {
    allocator_->FreeWithSize(ptr, n * sizeof(value_type));
  }
  template <typename U>
  struct rebind {
    typedef StdDynamicAllocator<U> other;
  };

  Allocator* allocator() const { return allocator_; }

 private:
  Allocator* allocator_;
};

}  // namespace nb

#endif  // NB_STD_ALLOCATOR_H_
