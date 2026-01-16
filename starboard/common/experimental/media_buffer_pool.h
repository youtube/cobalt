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

#ifndef STARBOARD_COMMON_EXPERIMENTAL_MEDIA_BUFFER_POOL_H_
#define STARBOARD_COMMON_EXPERIMENTAL_MEDIA_BUFFER_POOL_H_

#include <memory>
#include <type_traits>

#include "starboard/common/check_op.h"
#include "starboard/extension/experimental/media_buffer_pool.h"

namespace starboard {
namespace common {
namespace experimental {

// The media buffer pool is a linear space that cannot be directly accessed as
// memory, i.e. it cannot be read from or written to via memcpy().
// To avoid introducing a new SbPlayerWriteSamples() that works with the
// block allocated from the media buffer pool, the "pointers" allocated from the
// media buffer pool is annotated, so any code processing it can distinguish it
// from a pointer.
// The least significant bit of an annotated pointer is always 1, which is
// enough to be distinguished from in-memory pointers, which are always aligned
// to sizeof(void*).  The cast is technically unsafe as it changes the alias of
// the resulting pointers, but works across the compilers used by Cobalt.
inline void* AnnotatePointer(void* p) {
  if (p == nullptr) {
    return nullptr;
  }

  uintptr_t p_as_uint = reinterpret_cast<uintptr_t>(p);

  SB_DCHECK_EQ(p_as_uint % sizeof(void*), 0);
  // Ensure that the most significant bit is 0, as it will be lost when the
  // pointer is shifted left for annotation.
  SB_DCHECK_EQ(p_as_uint >> (sizeof(uintptr_t) * 8 - 1), 0u);

  p_as_uint = (p_as_uint << 1) + 1;

  return reinterpret_cast<void*>(p_as_uint);
}

// Pointer can be types like `void*` or `intptr_t`.
template <typename Pointer>
inline bool IsPointerAnnotated(Pointer p) {
  static_assert(sizeof(p) == sizeof(void*));

  uintptr_t p_as_uint;

  if constexpr (std::is_pointer_v<Pointer>) {
    p_as_uint = reinterpret_cast<uintptr_t>(p);
  } else {
    p_as_uint = static_cast<uintptr_t>(p);
  }

  if (p_as_uint % 2 == 0) {
    SB_DCHECK_EQ(p_as_uint % sizeof(void*), 0);
    return false;
  }

  p_as_uint >>= 1;

  SB_DCHECK_EQ(p_as_uint % sizeof(void*), 0);
  return true;
}

// Pointer can be types like `void*` or `intptr_t`.
template <typename Pointer>
inline Pointer UnannotatePointer(Pointer p) {
  if (p == 0) {
    return 0;
  }

  SB_DCHECK(IsPointerAnnotated(p));

  uintptr_t p_as_uint;

  if constexpr (std::is_pointer_v<Pointer>) {
    p_as_uint = reinterpret_cast<uintptr_t>(p);
    p_as_uint >>= 1;
    return reinterpret_cast<Pointer>(p_as_uint);
  } else {
    p_as_uint = static_cast<uintptr_t>(p);
    p_as_uint >>= 1;
    return static_cast<Pointer>(p_as_uint);
  }
}

// MediaBufferPool is a helper class that provides a C++ interface to the
// StarboardExtensionMediaBufferPoolApi. It allows for managing a shared memory
// pool for media buffers.
class MediaBufferPool {
 public:
  // Returns the single instance of MediaBufferPool that provides access to the
  // underlying system media buffer pool. Returns nullptr if the required
  // Starboard extension is not available on the current platform.
  static MediaBufferPool* Acquire();

  ~MediaBufferPool() = default;

  MediaBufferPool(const MediaBufferPool&) = delete;
  MediaBufferPool& operator=(const MediaBufferPool&) = delete;

  // Resets the size of the memory pool to zero, potentially releasing all
  // allocated memory.
  void ShrinkToZero();

  // Increases the memory pool's capacity to at least |size|. Returns |true| if
  // successful. Does nothing and returns |true| if the current capacity is
  // already greater than or equal to |size|.
  bool ExpandTo(size_t size);

  // Writes |size| bytes from |data| into the memory pool starting at
  // |position|. The caller must ensure that:
  // 1. |position| is annotated.
  // 2. [|position|, |position| + |size|), once unannotated, is within the
  // current capacity of the pool. This operation is guaranteed to succeed.
  void Write(intptr_t position, const void* data, size_t size);

 private:
  explicit MediaBufferPool(const StarboardExtensionMediaBufferPoolApi* api);

  const StarboardExtensionMediaBufferPoolApi* api_;
};

}  // namespace experimental
}  // namespace common
}  // namespace starboard

#endif  // STARBOARD_COMMON_EXPERIMENTAL_MEDIA_BUFFER_POOL_H_
