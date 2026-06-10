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

#include "build/build_config.h"
#include "starboard/shared/starboard/cached_feature.h"
#include "starboard/shared/starboard/feature_list.h"
#include "starboard/shared/starboard/player/fixed_block_pool.h"
#include "starboard/shared/starboard/player/lazy_initializer.h"

#if BUILDFLAG(IS_ANDROID)
#include "starboard/shared/starboard/features.h"
#endif

namespace starboard {
namespace {

#if BUILDFLAG(IS_ANDROID)
features::CachedFeature g_use_buffer_pool_cached(
    features::kDecodedAudioBufferPool);
#endif

// kBufferKiB (80) is chosen to be large enough to hold the maximum typical
// float32 PCM audio frame payloads, preventing frequent heap fallbacks:
// - Stereo Float32 PCM at 48kHz (20ms packets) requires 7,680 bytes.
// - 5.1 Surround Float32 PCM at 48kHz (20ms packets) requires 23,040 bytes.
// - Opus software decoder initial frame safety buffer requires 76,800 bytes.
// - Large renderer pull sizes (up to 66,560 bytes) driven by platform-specific
//   audio sink minimum buffer requirements (e.g., 96KB AudioTrack on Android).
constexpr size_t kBufferKiB = 80;

// kPoolSize (40) provides a safe margin. Real-world playback tests show a
// maximum of ~26 buffers in-flight concurrently (leaving 14+ free), while
// consuming ~3.12MiB of RAM total (80KiB * 40).
constexpr size_t kPoolSize = 40;

LazyInitializer<FixedBlockPool, /*NoDestruct=*/true> g_buffer_pool;

FixedBlockPool* GetPool() {
  return g_buffer_pool.Get("DecodedAudioBuffer", kBufferKiB * 1024, kPoolSize);
}

bool UseBufferPool() {
#if BUILDFLAG(IS_ANDROID)
  return g_use_buffer_pool_cached.IsEnabled();
#else
  if (features::FeatureList::HasOverrideForTesting("DecodedAudioBufferPool")) {
    return features::FeatureList::IsEnabledByName("DecodedAudioBufferPool");
  }
  return false;
#endif
}
}  // namespace

size_t Buffer::GetPoolFreeListSizeForTesting() {
  return GetPool()->GetInfoForTesting().free_blocks;
}

size_t Buffer::GetPoolTotalBlocksForTesting() {
  return GetPool()->GetInfoForTesting().total_blocks;
}

void Buffer::ClearCacheForTesting() {
#if BUILDFLAG(IS_ANDROID)
  g_use_buffer_pool_cached.ClearCacheForTesting();
#endif
}

uint8_t* Buffer::AllocateData(size_t size) {
  if (size == 0) {
    return nullptr;
  }

  if (UseBufferPool()) {
    return static_cast<uint8_t*>(GetPool()->Allocate(size));
  }
  return static_cast<uint8_t*>(::operator new(size));
}

void Buffer::FreeData(uint8_t* ptr) {
  if (!ptr) {
    return;
  }

  FixedBlockPool* pool = g_buffer_pool.GetIfInitialized();
  if (pool) {
    pool->Free(ptr);
    return;
  }
  ::operator delete(ptr);
}

}  // namespace starboard
