// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/starboard/player/object_pool.h"

#include <new>

namespace starboard {

ObjectPool::ObjectPool(size_t element_size, size_t capacity)
    : pool_(element_size, capacity) {}

void* ObjectPool::Allocate() {
  void* ptr = pool_.Allocate();
  if (ptr) {
    return ptr;
  }
  return ::operator new(pool_.block_size());
}

void ObjectPool::Free(void* ptr) {
  if (!ptr) {
    return;
  }

  if (pool_.IsFromPool(ptr)) {
    pool_.Free(ptr);
  } else {
    ::operator delete(ptr);
  }
}

bool ObjectPool::IsFromPreallocatedPool(void* ptr) const {
  return pool_.IsFromPool(ptr);
}

size_t ObjectPool::element_size() const {
  return pool_.block_size();
}

size_t ObjectPool::capacity() const {
  return pool_.capacity();
}

size_t ObjectPool::free_list_size() const {
  return pool_.free_list_size();
}

}  // namespace starboard
