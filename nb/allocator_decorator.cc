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

#include "nb/allocator_decorator.h"

#include "starboard/mutex.h"

namespace nb {

class AllocatorDecorator::Lock {
 public:
  void Acquire() { mutex_.Acquire(); }
  void Release() { mutex_.Release(); }

 private:
  starboard::Mutex mutex_;
};

class AllocatorDecorator::ScopedLock {
 public:
  ScopedLock(Lock* lock) : lock_(lock) {
    if (lock_) {
      lock_->Acquire();
    }
  }
  ~ScopedLock() {
    if (lock_) {
      lock_->Release();
    }
  }

 private:
  Lock* lock_;
};

AllocatorDecorator::AllocatorDecorator(scoped_ptr<Allocator> impl,
                                       bool thread_safe)
    : lock_(NULL), impl_(impl.Pass()) {
  if (thread_safe) {
    lock_ = new Lock;
  }
}

AllocatorDecorator::~AllocatorDecorator() {
  delete lock_;
}

void* AllocatorDecorator::Allocate(std::size_t size) {
  ScopedLock scoped_lock(lock_);
  return impl_->Allocate(size);
}

void* AllocatorDecorator::Allocate(std::size_t size, std::size_t alignment) {
  ScopedLock scoped_lock(lock_);
  return impl_->Allocate(size, alignment);
}

void* AllocatorDecorator::AllocateForAlignment(std::size_t size,
                                               std::size_t alignment) {
  ScopedLock scoped_lock(lock_);
  return impl_->AllocateForAlignment(size, alignment);
}

void AllocatorDecorator::Free(void* memory) {
  ScopedLock scoped_lock(lock_);
  impl_->Free(memory);
}

std::size_t AllocatorDecorator::GetCapacity() const {
  ScopedLock scoped_lock(lock_);
  return impl_->GetCapacity();
}

std::size_t AllocatorDecorator::GetAllocated() const {
  ScopedLock scoped_lock(lock_);
  return impl_->GetAllocated();
}

void AllocatorDecorator::PrintAllocations() const {
  ScopedLock scoped_lock(lock_);
  impl_->PrintAllocations();
}

}  // namespace nb
