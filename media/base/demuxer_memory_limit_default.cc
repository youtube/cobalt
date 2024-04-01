// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/demuxer_memory_limit.h"

namespace media {

size_t GetDemuxerStreamAudioMemoryLimit(
    const AudioDecoderConfig* /*audio_config*/) {
  return internal::kDemuxerStreamAudioMemoryLimitDefault;
}

size_t GetDemuxerStreamVideoMemoryLimit(
    Demuxer::DemuxerTypes /*demuxer_type*/,
    const VideoDecoderConfig* /*video_config*/) {
  return internal::kDemuxerStreamVideoMemoryLimitDefault;
}

size_t GetDemuxerMemoryLimit(Demuxer::DemuxerTypes demuxer_type) {
  return GetDemuxerStreamAudioMemoryLimit(nullptr) +
         GetDemuxerStreamVideoMemoryLimit(demuxer_type, nullptr);
}

}  // namespace media
