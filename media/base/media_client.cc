// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/media_client.h"


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
uint64_t MediaClient::GetMediaSourceSizeLimit() {
  if (g_media_client) {
    return g_media_client->GetMaximumMemoryCapacity();
  }
  return 0;
}

// static
uint64_t MediaClient::GetTotalMediaSourceSize() {
  if (g_media_client) {
    return g_media_client->GetCurrentMemoryCapacity();
  }
  return 0;
}

// static
uint64_t MediaClient::GetUsedMediaSourceMemorySize() {
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
