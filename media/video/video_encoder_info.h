// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_VIDEO_ENCODER_INFO_H_
#define MEDIA_VIDEO_VIDEO_ENCODER_INFO_H_

#include <stdint.h>
#include <array>
#include <string>
#include <vector>

#include "media/base/media_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/size.h"

namespace media {

// These chromium classes are the corresponding classes in webrtc project.
// See third_party/webrtc/api/video_codecs/video_encoder.h for the detail.

struct MEDIA_EXPORT ResolutionBitrateLimit {
  ResolutionBitrateLimit();
  ResolutionBitrateLimit(const ResolutionBitrateLimit&);
  ResolutionBitrateLimit(const gfx::Size& frame_size,
                         int min_start_bitrate_bps,
                         int min_bitrate_bps,
                         int max_bitrate_bps);
  ~ResolutionBitrateLimit();

  friend bool operator==(const ResolutionBitrateLimit&,
                         const ResolutionBitrateLimit&) = default;

  gfx::Size frame_size;
  int min_start_bitrate_bps = 0;
  int min_bitrate_bps = 0;
  int max_bitrate_bps = 0;
};

struct MEDIA_EXPORT VideoEncoderInfo {
  static constexpr size_t kMaxSpatialLayers = 5;

  VideoEncoderInfo();
  VideoEncoderInfo(const VideoEncoderInfo&);
  ~VideoEncoderInfo();

  friend bool operator==(const VideoEncoderInfo&,
                         const VideoEncoderInfo&) = default;

  std::string implementation_name;

  // The number of additional input frames that must be enqueued before the
  // encoder starts producing output for the first frame, i.e., the size of the
  // compression window. Equal to 0 if the encoder can produce a chunk of
  // output just from the frame submitted last.
  // If absent, the encoder client will assume some default value.
  absl::optional<int> frame_delay;

  // The number of input frames the encoder can queue internally. Once this
  // number is reached, further encode requests can block until some output has
  // been produced.
  // If absent, the encoder client will assume some default value.
  absl::optional<int> input_capacity;

  bool supports_native_handle = true;
  bool has_trusted_rate_controller = false;
  bool is_hardware_accelerated = true;
  bool supports_simulcast = false;
  // True if encoder uses same QP for all macroblocks of a picture without
  // per-macroblock QP adjustment, and that QP can be calculated from
  // uncompressed sequence/frame/slice/tile headers.
  bool reports_average_qp = true;
  uint32_t requested_resolution_alignment = 1;
  bool apply_alignment_to_all_simulcast_layers = false;

  std::array<std::vector<uint8_t>, kMaxSpatialLayers> fps_allocation;
  std::vector<ResolutionBitrateLimit> resolution_bitrate_limits;
};

}  // namespace media

#endif  // MEDIA_VIDEO_VIDEO_ENCODER_INFO_H_
