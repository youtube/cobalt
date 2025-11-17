// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/media_client.h"

#if BUILDFLAG(USE_STARBOARD_MEDIA)
#include "media/base/decoder_buffer.h"
#endif

namespace media {

static MediaClient* g_media_client = nullptr;

void SetMediaClient(MediaClient* media_client) {
  g_media_client = media_client;
}

MediaClient* GetMediaClient() {
  return g_media_client;
}

MediaClient::MediaClient() {
#if BUILDFLAG(USE_STARBOARD_MEDIA)
  DecoderBuffer::Allocator::Set(&decoder_buffer_allocator_);
#endif
}

MediaClient::~MediaClient() = default;

#if BUILDFLAG(USE_STARBOARD_MEDIA)
// static
uint64_t MediaClient::GetMediaSourceMaximumMemoryCapacity() {
  if (g_media_client) {
    return g_media_client->GetMaximumMemoryCapacity();
  }
  return 0;
}

// static
uint64_t MediaClient::GetMediaSourceCurrentMemoryCapacity() {
  if (g_media_client) {
    return g_media_client->GetCurrentMemoryCapacity();
  }
  return 0;
}

// static
uint64_t MediaClient::GetMediaSourceTotalAllocatedMemory() {
  if (g_media_client) {
    return g_media_client->GetAllocatedMemory();
  }
  return 0;
}

uint64_t MediaClient::GetMaximumMemoryCapacity() const {
  return decoder_buffer_allocator_.GetMaximumMemoryCapacity();
}

uint64_t MediaClient::GetCurrentMemoryCapacity() const {
  return decoder_buffer_allocator_.GetCurrentMemoryCapacity();
}

uint64_t MediaClient::GetAllocatedMemory() const {
  return decoder_buffer_allocator_.GetAllocatedMemory();
}
#endif  // BUILDFLAG(USE_STARBOARD_MEDIA)

}  // namespace media
