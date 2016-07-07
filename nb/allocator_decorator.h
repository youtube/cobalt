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
#include "starboard/mutex.h"

namespace nb {

class ThreadSafe {
 public:
  void Lock() { mutex_.Acquire(); }
  void Unlock() { mutex_.Release(); }

 private:
  starboard::Mutex mutex_;
};

class ThreadUnsafe {
 public:
  void Lock() {}
  void Unlock() {}
};

// Templatized class configured with an allocator behavior
// and threading behavior.
// Dispatches Allocate and Free calls to the internal implementation.
// Threadsafe or not, depending on the LockTraits.
// In non-release builds, provides a cval tracking heap size.
template <typename LockTraits, typename AllocatorImpl>
class AllocatorDecorator : public Allocator {
 public:
  explicit AllocatorDecorator(scoped_ptr<AllocatorImpl> impl)
      : impl_(impl.Pass()) {}

  void* Allocate(size_t size) {
    mutex_.Lock();
    void* ret = impl_->AllocatorImpl::Allocate(size);
    mutex_.Unlock();
    return ret;
  }
  void* Allocate(size_t size, size_t alignment) {
    mutex_.Lock();
    void* ret = impl_->AllocatorImpl::Allocate(size, alignment);
    mutex_.Unlock();
    return ret;
  }
  void* AllocateForAlignment(size_t size, size_t alignment) {
    mutex_.Lock();
    void* ret = impl_->AllocatorImpl::AllocateForAlignment(size, alignment);
    mutex_.Unlock();
    return ret;
  }
  void Free(void* memory) {
    mutex_.Lock();
    impl_->AllocatorImpl::Free(memory);
    mutex_.Unlock();
  }
  size_t GetCapacity() const {
    mutex_.Lock();
    size_t result = impl_->AllocatorImpl::GetCapacity();
    mutex_.Unlock();
    return result;
  }
  size_t GetAllocated() const {
    mutex_.Lock();
    size_t result = impl_->AllocatorImpl::GetAllocated();
    mutex_.Unlock();
    return result;
  }

  // Expose the internals if necessary.
  AllocatorImpl* GetAllocatorImpl() { return impl_.get(); }
  const AllocatorImpl* GetAllocatorImpl() const { return impl_.get(); }

 private:
  mutable LockTraits mutex_;
  scoped_ptr<AllocatorImpl> impl_;
};

}  // namespace nb

#endif  // NB_ALLOCATOR_DECORATOR_H_
