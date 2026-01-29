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

#ifndef STARBOARD_ANDROID_SHARED_MEMFD_MEDIA_BUFFER_POOL_H_
#define STARBOARD_ANDROID_SHARED_MEMFD_MEDIA_BUFFER_POOL_H_

#include <stddef.h>
#include <stdint.h>

namespace starboard {
namespace android {
namespace shared {

// MemFdMediaBufferPool manages a shared memory pool using memfd_create.
class MemFdMediaBufferPool {
 public:
  static MemFdMediaBufferPool* Get();

  // Resets the memory pool's capacity to zero and truncates the underlying
  // file. This may release the associated physical memory.
  // The caller must ensure that no concurrent Write() or Read() operations
  // are in progress when this is called.
  void ShrinkToZero();

  bool ExpandTo(size_t size);
  void Write(intptr_t position, const void* data, size_t size);
  void Read(intptr_t position, void* buffer, size_t size);

 private:
  MemFdMediaBufferPool();
  ~MemFdMediaBufferPool();

  MemFdMediaBufferPool(const MemFdMediaBufferPool&) = delete;
  MemFdMediaBufferPool& operator=(const MemFdMediaBufferPool&) = delete;

  int fd_ = -1;
  size_t current_capacity_ = 0;
};

}  // namespace shared
}  // namespace android
}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_MEMFD_MEDIA_BUFFER_POOL_H_
