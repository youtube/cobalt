/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/utility/frame_sampler.h"

#include "api/video/video_frame.h"
#include "modules/include/module_common_types_public.h"

namespace webrtc {

constexpr int kTimestampDifference =
    90'000 - 1;  // Sample every 90khz or once per second.

bool FrameSampler::ShouldBeSampled(const VideoFrame& frame) {
  if (!last_rtp_timestamp_sampled_.has_value() ||
      (IsNewerTimestamp(frame.rtp_timestamp(),
                        *last_rtp_timestamp_sampled_ + kTimestampDifference))) {
    last_rtp_timestamp_sampled_ = frame.rtp_timestamp();
    return true;
  }
  return false;
}

}  // namespace webrtc
