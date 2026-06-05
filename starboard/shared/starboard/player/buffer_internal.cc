// Copyright 2026 The Cobalt Authors. All Rights Reserved.
// Force rebuild.
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

#include "starboard/shared/starboard/player/buffer_internal.h"

#include <algorithm>
#include <cstring>
#include <utility>

#include "build/build_config.h"
#include "starboard/common/check_op.h"
#include "starboard/common/log.h"
#include "starboard/shared/starboard/feature_list.h"
#include "starboard/shared/starboard/player/fixed_size_memory_pool.h"
#include "starboard/shared/starboard/player/lazy_initializer.h"

#if BUILDFLAG(IS_ANDROID)
#include "starboard/shared/starboard/features.h"
#endif

namespace starboard {
namespace {
// kBufferSize (32KB) is chosen to be large enough to hold the maximum typical
// float32 PCM audio frame payloads:
// - Stereo Float32 PCM at 48kHz (20ms packets) requires 7,680 bytes.
// - 5.1 Surround Float32 PCM at 48kHz (20ms packets) requires 23,040 bytes.
constexpr size_t kBufferSize = 32 * 1024;

// kPoolSize (40) provides a safe margin. Real-world playback tests show a
// maximum of ~26 buffers in-flight concurrently (leaving 14+ free), while
// consuming only ~1.25MB of RAM total (32KB * 40).
constexpr size_t kPoolSize = 40;

LazyInitializer<FixedSizeMemoryPool, /*NoDestruct=*/true> g_buffer_pool;

FixedSizeMemoryPool* GetPool() {
  return g_buffer_pool.Get(kBufferSize, kPoolSize);
}

bool UseBufferPool() {
#if BUILDFLAG(IS_ANDROID)
  return features::FeatureList::IsEnabled(
      features::kEnableDecodedAudioBufferPool);
#else
  if (features::FeatureList::HasOverrideForTesting(
          "EnableDecodedAudioBufferPool")) {
    return features::FeatureList::IsEnabledByName(
        "EnableDecodedAudioBufferPool");
  }
  return false;
#endif
}
}  // namespace

size_t Buffer::GetPoolFreeListSizeForTesting() {
  return GetPool()->free_list_size();
}

size_t Buffer::GetPoolCapacityForTesting() {
  return GetPool()->capacity();
}

uint8_t* Buffer::AllocateData(size_t size) {
  if (size == 0) {
    return nullptr;
  }

  if (UseBufferPool()) {
    FixedSizeMemoryPool* pool = GetPool();
    if (size <= pool->block_size()) {
      void* ptr = pool->Allocate();
      if (ptr) {
        return static_cast<uint8_t*>(ptr);
      }
    }
  }
  return new uint8_t[size];
}

void Buffer::FreeData(uint8_t* ptr) {
  if (!ptr) {
    return;
  }

  FixedSizeMemoryPool* pool = g_buffer_pool.GetIfInitialized();
  if (pool && pool->IsFromPool(ptr)) {
    pool->Free(ptr);
    return;
  }
  delete[] ptr;
}

}  // namespace starboard
