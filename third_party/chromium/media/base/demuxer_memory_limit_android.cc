// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/demuxer_memory_limit.h"

#include "base/android/build_info.h"
#include "base/system/sys_info.h"

namespace media {

namespace {

size_t SelectLimit(size_t default_limit,
                   size_t low_limit,
                   size_t very_low_limit) {
  if (!base::SysInfo::IsLowEndDevice()) {
    return default_limit;
  }
  // Use very low limit on 512MiB Android Go devices only.
  if (base::android::BuildInfo::GetInstance()->sdk_int() >=
          base::android::SDK_VERSION_OREO &&
      base::SysInfo::AmountOfPhysicalMemoryMB() <= 512) {
    return very_low_limit;
  }
  return low_limit;
}

}  // namespace

size_t GetDemuxerStreamAudioMemoryLimit(
    const AudioDecoderConfig* /*audio_config*/) {
  static const size_t limit =
      SelectLimit(internal::kDemuxerStreamAudioMemoryLimitDefault,
                  internal::kDemuxerStreamAudioMemoryLimitLow,
                  internal::kDemuxerStreamAudioMemoryLimitVeryLow);
  return limit;
}

size_t GetDemuxerStreamVideoMemoryLimit(
    Demuxer::DemuxerTypes /*demuxer_type*/,
    const VideoDecoderConfig* /*video_config*/) {
  static const size_t limit =
      SelectLimit(internal::kDemuxerStreamVideoMemoryLimitDefault,
                  internal::kDemuxerStreamVideoMemoryLimitLow,
                  internal::kDemuxerStreamVideoMemoryLimitVeryLow);
  return limit;
}

size_t GetDemuxerMemoryLimit(Demuxer::DemuxerTypes demuxer_type) {
  return GetDemuxerStreamAudioMemoryLimit(nullptr) +
         GetDemuxerStreamVideoMemoryLimit(demuxer_type, nullptr);
}

}  // namespace media
