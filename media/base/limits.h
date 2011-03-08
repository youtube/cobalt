// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains limit definition constants for the media subsystem.

#ifndef MEDIA_BASE_LIMITS_H_
#define MEDIA_BASE_LIMITS_H_

#include "base/basictypes.h"

namespace media {

struct Limits {
  // For video.
  static const int kMaxDimension = (1 << 15) - 1;  // 32767
  static const int kMaxCanvas = (1 << (14 * 2));  // 16384 x 16384

  // Total number of video frames which are populating in the pipeline.
  static const size_t kMaxVideoFrames = 4;

  // Following limits are used by AudioParameters::IsValid().
  // The 192 Khz constant is the frequency of quicktime lossless audio codec.
  // MP4 is limited to 96 Khz, and mp3 is limited to 48 Khz.
  // OGG vorbis was initially limited to 96 Khz, but recent tools are unlimited.
  // 192 Khz is also the limit on most PC audio hardware.
  static const int kMaxSampleRate = 192000;
  static const int kMaxChannels = 32;
  static const int kMaxBitsPerSample = 64;
  static const int kMaxSamplesPerPacket = kMaxSampleRate;

  // Maximum possible time.
  static const int64 kMaxTimeInMicroseconds = kint64max;
};

}  // namespace media

#endif  // MEDIA_BASE_LIMITS_H_
