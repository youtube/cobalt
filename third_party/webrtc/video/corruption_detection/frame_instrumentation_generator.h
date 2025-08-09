/*
 * Copyright 2024 The WebRTC project authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS.  All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_CORRUPTION_DETECTION_FRAME_INSTRUMENTATION_GENERATOR_H_
#define VIDEO_CORRUPTION_DETECTION_FRAME_INSTRUMENTATION_GENERATOR_H_

#include <cstdint>
#include <map>
#include <optional>
#include <queue>

#include "absl/types/variant.h"
#include "api/video/encoded_image.h"
#include "api/video/video_codec_type.h"
#include "api/video/video_frame.h"
#include "common_video/frame_instrumentation_data.h"
#include "video/corruption_detection/halton_frame_sampler.h"

namespace webrtc {

class FrameInstrumentationGenerator {
 public:
  FrameInstrumentationGenerator() = delete;
  explicit FrameInstrumentationGenerator(VideoCodecType video_codec_type);

  FrameInstrumentationGenerator(const FrameInstrumentationGenerator&) = delete;
  FrameInstrumentationGenerator& operator=(
      const FrameInstrumentationGenerator&) = delete;

  ~FrameInstrumentationGenerator() = default;

  void OnCapturedFrame(VideoFrame frame);
  std::optional<
      absl::variant<FrameInstrumentationSyncData, FrameInstrumentationData>>
  OnEncodedImage(const EncodedImage& encoded_image);

  // Returns `std::nullopt` if there is no context for the given layer.
  std::optional<int> GetHaltonSequenceIndex(int layer_id) const;
  void SetHaltonSequenceIndex(int index, int layer_id);

  int GetLayerId(const EncodedImage& encoded_image) const;

 private:
  struct Context {
    HaltonFrameSampler frame_sampler;
    uint32_t rtp_timestamp_of_last_key_frame = 0;
  };

  // Incoming video frames in capture order.
  std::queue<VideoFrame> captured_frames_;
  // Map from spatial or simulcast index to sampling context.
  std::map<int, Context> contexts_;
  const VideoCodecType video_codec_type_;
};

}  // namespace webrtc

#endif  // VIDEO_CORRUPTION_DETECTION_FRAME_INSTRUMENTATION_GENERATOR_H_
