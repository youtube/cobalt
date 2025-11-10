// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/media_client.h"

#if BUILDFLAG(USE_STARBOARD_MEDIA)
#include "media/base/decoder_buffer.h"
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
  return DecoderBuffer::Allocator::GetGlobalMaximumMemoryCapacity();
}

// static
uint64_t MediaClient::GetMediaSourceCurrentMemoryCapacity() {
  return DecoderBuffer::Allocator::GetGlobalCurrentMemoryCapacity();
}

// static
uint64_t MediaClient::GetMediaSourceTotalAllocatedMemory() {
  return DecoderBuffer::Allocator::GetGlobalAllocatedMemory();
}

#endif  // BUILDFLAG(USE_STARBOARD_MEDIA)

}  // namespace media
