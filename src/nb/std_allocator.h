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
  StdAllocator(const StdAllocator<U, V>& x) {}

  pointer allocate(size_type n,
                   std::allocator<void>::const_pointer hint = NULL) {
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

}  // namespace nb

#endif  // NB_STD_ALLOCATOR_H_
