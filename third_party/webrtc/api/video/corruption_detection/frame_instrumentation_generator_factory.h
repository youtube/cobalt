/*
 * Copyright 2025 The WebRTC project authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS.  All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_CORRUPTION_DETECTION_FRAME_INSTRUMENTATION_GENERATOR_FACTORY_H_
#define API_VIDEO_CORRUPTION_DETECTION_FRAME_INSTRUMENTATION_GENERATOR_FACTORY_H_

#include <memory>

#include "api/video/corruption_detection/frame_instrumentation_generator.h"
#include "api/video/video_codec_type.h"

namespace webrtc {

class FrameInstrumentationGeneratorFactory {
 public:
  static std::unique_ptr<FrameInstrumentationGenerator> Create(
      VideoCodecType video_codec_type);
};

}  // namespace webrtc

#endif  // API_VIDEO_CORRUPTION_DETECTION_FRAME_INSTRUMENTATION_GENERATOR_FACTORY_H_
