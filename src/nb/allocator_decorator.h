/*
 * Copyright 2014 Google Inc. All Rights Reserved.
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

#ifndef NB_ALLOCATOR_DECORATOR_H_
#define NB_ALLOCATOR_DECORATOR_H_

#include "nb/allocator.h"
#include "nb/scoped_ptr.h"

namespace nb {

// Class that can be configured with an allocator behavior and threading
// behavior.
// Dispatches Allocate and Free calls to the internal implementation.
// Threadsafe or not, depending on the LockTraits.
// In non-release builds, provides a cval tracking heap size.
class AllocatorDecorator : public Allocator {
 public:
  AllocatorDecorator(scoped_ptr<Allocator> impl, bool thread_safe);
  ~AllocatorDecorator();

  void* Allocate(std::size_t size);
  void* Allocate(std::size_t size, std::size_t alignment);
  void* AllocateForAlignment(std::size_t size, std::size_t alignment);
  void Free(void* memory);
  std::size_t GetCapacity() const;
  std::size_t GetAllocated() const;
  void PrintAllocations() const;

 private:
  class Lock;
  class ScopedLock;

  Lock* lock_;
  scoped_ptr<Allocator> impl_;
};

}  // namespace nb

#endif  // NB_ALLOCATOR_DECORATOR_H_
