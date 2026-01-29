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

#include "media/starboard/media_buffer_pool_decoder_buffer_allocator_strategy.h"

#include <limits>

#include "media/base/demuxer_stream.h"
#include "starboard/common/experimental/media_buffer_pool.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace {

using starboard::common::experimental::MediaBufferPool;

TEST(MediaBufferPoolDecoderBufferAllocatorStrategyTest,
     AllocateHugeBufferReturnsNull) {
  constexpr size_t kAlignment = sizeof(void*);

  MediaBufferPool* pool = MediaBufferPool::Acquire();

  if (!pool) {
    return;
  }

  MediaBufferPoolDecoderBufferAllocatorStrategy strategy(pool, 1024, 1024);

  // Use a huge size that is likely to fail allocation.
  const size_t kHugeSize = std::numeric_limits<size_t>::max() / 2;

  // We don't verify audio allocation as its current implementation CHECKs on
  // failure.  It's not important as the implementation is similar to existing
  // allocators.

  // Verify huge video allocation returns nullptr.
  EXPECT_EQ(strategy.Allocate(DemuxerStream::VIDEO, kHugeSize, kAlignment),
            nullptr);
}

}  // namespace
}  // namespace media
