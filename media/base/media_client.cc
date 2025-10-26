// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/media_client.h"

#if BUILDFLAG(USE_STARBOARD_MEDIA)
#include "media/base/decoder_buffer.h"
#include "media/starboard/decoder_buffer_allocator.h"
#endif  // BUILDFLAG(USE_STARBOARD_MEDIA)

namespace media {

static MediaClient* g_media_client = nullptr;

void SetMediaClient(MediaClient* media_client) {
  g_media_client = media_client;
}

MediaClient* GetMediaClient() {
  return g_media_client;
}

MediaClient::MediaClient() = default;

MediaClient::~MediaClient() = default;

#if BUILDFLAG(USE_STARBOARD_MEDIA)
// static
uint64_t MediaClient::GetMediaSourceMaximumMemoryCapacity() {
  auto allocator = static_cast<DecoderBufferAllocator*>(
      DecoderBuffer::Allocator::GetInstance());
  if (allocator == nullptr) {
    return 0;
  }
  return allocator->GetMaximumMemoryCapacity();
}

// static
uint64_t MediaClient::GetMediaSourceCurrentMemoryCapacity() {
  auto allocator = static_cast<DecoderBufferAllocator*>(
      DecoderBuffer::Allocator::GetInstance());
  if (allocator == nullptr) {
    return 0;
  }
  return allocator->GetCurrentMemoryCapacity();
}

// static
uint64_t MediaClient::GetMediaSourceTotalAllocatedMemory() {
  auto allocator = static_cast<DecoderBufferAllocator*>(
      DecoderBuffer::Allocator::GetInstance());
  if (allocator == nullptr) {
    return 0;
  }
  return allocator->GetAllocatedMemory();
}

#endif  // BUILDFLAG(USE_STARBOARD_MEDIA)

}  // namespace media
