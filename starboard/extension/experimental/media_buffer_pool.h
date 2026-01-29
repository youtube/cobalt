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

#ifndef STARBOARD_EXTENSION_EXPERIMENTAL_MEDIA_BUFFER_POOL_H_
#define STARBOARD_EXTENSION_EXPERIMENTAL_MEDIA_BUFFER_POOL_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// WARNING: Please DO NOT implement or make use of this extension.  The
//          extension could be changed or removed at any time, even within the
//          same Cobalt/Starboard version
//
// The extension SHOULD NOT be used directly.  It should be used via its wrapper
// in //starboard/common/experimental/media_buffer_pool.h, which contains code
// to handle the co-existence of pointers allocated from the memory pool and
// from heap.

#define kStarboardExtensionMediaBufferPoolApiName \
  "dev.starboard.extension.MediaBufferPool"

typedef struct StarboardExtensionMediaBufferPoolApi {
  // Name should be the string |kStarboardExtensionMediaBufferPoolApiName|.
  // This helps to validate that the extension API is correct.
  const char* name;

  // This specifies the version of the API that is implemented.
  uint32_t version;

  // All functions below can be called from any threads.  However, the caller is
  // responsible for ensuring the logical coordination of calls. For example,
  // the caller must ensure that ShrinkToZero() is only called after all
  // concurrent Write() operations have completed and that no new Write()
  // operations are initiated until a subsequent ExpandTo() call has completed.

  // Resets the memory pool's capacity to zero. This allows the underlying
  // implementation to reclaim any physical memory associated with the pool.
  void (*ShrinkToZero)();

  // Increases the memory pool's capacity to at least |size|. Returns |true| if
  // successful. Does nothing and returns |true| if the current capacity is
  // already greater than or equal to |size|.
  bool (*ExpandTo)(size_t size);

  // Writes |size| bytes from |data| into the memory pool starting at
  // |position|. The caller must ensure that [position, position + size) is
  // within the current capacity of the pool. This operation is guaranteed to
  // succeed.
  void (*Write)(intptr_t position, const void* data, size_t size);
} StarboardExtensionMediaBufferPoolApi;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_EXTENSION_EXPERIMENTAL_MEDIA_BUFFER_POOL_H_
