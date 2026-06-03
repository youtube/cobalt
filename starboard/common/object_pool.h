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

#ifndef STARBOARD_COMMON_OBJECT_POOL_H_
#define STARBOARD_COMMON_OBJECT_POOL_H_

#include <mutex>
#include <vector>

namespace starboard {
namespace common {

// A thread-safe, dynamic object pool for recycling memory allocations of type
// T. This pool helps eliminate steady-state heap allocation churn for
// frequently allocated/freed objects of the same type.
template <typename T>
class ObjectPool {
 public:
  static ObjectPool<T>* GetInstance() {
    static ObjectPool<T> instance;
    return &instance;
  }

  // Allocates a block of memory of sizeof(T).
  // Returns a recycled block from the pool if available, or allocates a new one
  // from heap.
  void* Allocate() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (free_list_.empty()) {
      return ::operator new(sizeof(T));
    }
    void* ptr = free_list_.back();
    free_list_.pop_back();
    return ptr;
  }

  // Returns a block of memory of sizeof(T) back to the pool for future reuse.
  void Free(void* ptr) {
    if (!ptr) {
      return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    free_list_.push_back(ptr);
  }

 private:
  ObjectPool() = default;
  ~ObjectPool() {
    for (void* ptr : free_list_) {
      ::operator delete(ptr);
    }
  }

  std::mutex mutex_;
  std::vector<void*> free_list_;

  ObjectPool(const ObjectPool&) = delete;
  ObjectPool& operator=(const ObjectPool&) = delete;
};

}  // namespace common
}  // namespace starboard

#endif  // STARBOARD_COMMON_OBJECT_POOL_H_
