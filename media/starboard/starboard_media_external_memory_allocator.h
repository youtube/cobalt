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

#ifndef MEDIA_STARBOARD_STARBOARD_MEDIA_EXTERNAL_MEMORY_ALLOCATOR_H_
#define MEDIA_STARBOARD_STARBOARD_MEDIA_EXTERNAL_MEMORY_ALLOCATOR_H_

#include <memory>

#include "base/containers/span.h"
#include "media/base/decoder_buffer.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_client.h"
#include "media/base/media_export.h"

namespace media {

// An ExternalMemoryAllocator implementation backed by Cobalt's Starboard
// media memory pool (DecoderBufferAllocator). This routes Chromium's
// stream parser frame allocations directly into Starboard pool memory.
//
// Lifetime/Ownership: Owned by the renderer client
// (CobaltContentRendererClient) via a std::unique_ptr and exposed to stream
// parsers via MediaClient::GetMediaAllocator(). The instance exists for the
// duration of the renderer process/thread lifecycle when enabled.
//
// Threading Model: Thread-safe for CopyFrom(). Because the underlying
// DecoderBufferAllocator is internally synchronized with a mutex, CopyFrom()
// can be safely called from any demuxer or stream parser thread.
class MEDIA_EXPORT StarboardMediaExternalMemoryAllocator
    : public ExternalMemoryAllocator {
 public:
  StarboardMediaExternalMemoryAllocator();
  ~StarboardMediaExternalMemoryAllocator() override;

  StarboardMediaExternalMemoryAllocator(
      const StarboardMediaExternalMemoryAllocator&) = delete;
  StarboardMediaExternalMemoryAllocator& operator=(
      const StarboardMediaExternalMemoryAllocator&) = delete;

  // ExternalMemoryAllocator implementation:
  std::unique_ptr<DecoderBuffer::ExternalMemory> CopyFrom(
      base::span<const uint8_t> span) override;
  std::unique_ptr<DecoderBuffer::ExternalMemory> CopyFrom(
      base::span<const uint8_t> span,
      DemuxerStream::Type type) override;
};

}  // namespace media

#endif  // MEDIA_STARBOARD_STARBOARD_MEDIA_EXTERNAL_MEMORY_ALLOCATOR_H_
