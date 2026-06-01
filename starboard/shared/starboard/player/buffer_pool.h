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

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_BUFFER_POOL_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_BUFFER_POOL_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

#include "starboard/common/log.h"

namespace starboard {

// BufferPool is a thread-safe, fixed-size memory pool used to allocate and
// reuse physical memory buffers for audio PCM data. This reduces heap churn
// during playback.
//
// Lifetime and Ownership:
// The pool is typically managed as a global/static singleton by the player
// implementation and persists for the lifetime of the application. Individual
// buffers allocated from the pool are tracked by starboard::Buffer and must
// be returned to the pool via Free() before the pool is destroyed.
//
// Threading Model:
// This class is fully thread-safe and its methods (Allocate, Free, IsFromPool)
// can be safely called from any thread concurrently.
class BufferPool {
 public:
  // Constructor. Public to allow unit tests to create isolated instances.
  BufferPool(size_t buffer_size, size_t pool_size);
  ~BufferPool();

  // Allocate a buffer. If size exceeds buffer_size or pool is empty,
  // it falls back to standard heap allocation.
  uint8_t* Allocate(size_t size);

  // Free a buffer. If it was allocated from the pool, returns it.
  // Otherwise, deletes it.
  void Free(uint8_t* ptr, size_t size);

  // Helper to check if a pointer belongs to this pool.
  bool IsFromPool(uint8_t* ptr) const;

  // Getters for testing
  size_t buffer_size() const { return buffer_size_; }
  size_t pool_size() const { return pool_size_; }
  size_t free_list_size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return free_list_.size();
  }

 private:
  const size_t buffer_size_;
  const size_t pool_size_;
  std::unique_ptr<uint8_t[]> pool_storage_;
  std::vector<uint8_t*> free_list_;
  mutable std::mutex mutex_;

  BufferPool(const BufferPool&) = delete;
  void operator=(const BufferPool&) = delete;
};

}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_BUFFER_POOL_H_
