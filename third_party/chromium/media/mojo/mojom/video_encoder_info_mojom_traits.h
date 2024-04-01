// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_MOJOM_VIDEO_ENCODER_INFO_MOJOM_TRAITS_H_
#define MEDIA_MOJO_MOJOM_VIDEO_ENCODER_INFO_MOJOM_TRAITS_H_

#include "base/containers/span.h"
#include "media/mojo/mojom/video_encoder_info.mojom-shared.h"
#include "media/video/video_encoder_info.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"

namespace mojo {

template <>
class StructTraits<media::mojom::ResolutionBitrateLimitDataView,
                   media::ResolutionBitrateLimit> {
 public:
  static const gfx::Size& frame_size(
      const media::ResolutionBitrateLimit& resolution_bitrate_limit) {
    return resolution_bitrate_limit.frame_size;
  }
  static int min_start_bitrate_bps(
      const media::ResolutionBitrateLimit& resolution_bitrate_limit) {
    return resolution_bitrate_limit.min_start_bitrate_bps;
  }
  static int min_bitrate_bps(
      const media::ResolutionBitrateLimit& resolution_bitrate_limit) {
    return resolution_bitrate_limit.min_bitrate_bps;
  }
  static int max_bitrate_bps(
      const media::ResolutionBitrateLimit& resolution_bitrate_limit) {
    return resolution_bitrate_limit.max_bitrate_bps;
  }
  static bool Read(media::mojom::ResolutionBitrateLimitDataView data,
                   media::ResolutionBitrateLimit* out);
};

template <>
class StructTraits<media::mojom::VideoEncoderInfoDataView,
                   media::VideoEncoderInfo> {
 public:
  static const std::string& implementation_name(
      const media::VideoEncoderInfo& video_encoder_info) {
    return video_encoder_info.implementation_name;
  }
  static bool supports_native_handle(
      const media::VideoEncoderInfo& video_encoder_info) {
    return video_encoder_info.supports_native_handle;
  }
  static bool has_trusted_rate_controller(
      const media::VideoEncoderInfo& video_encoder_info) {
    return video_encoder_info.has_trusted_rate_controller;
  }
  static bool is_hardware_accelerated(
      const media::VideoEncoderInfo& video_encoder_info) {
    return video_encoder_info.is_hardware_accelerated;
  }
  static bool supports_simulcast(
      const media::VideoEncoderInfo& video_encoder_info) {
    return video_encoder_info.supports_simulcast;
  }
  static base::span<const std::vector<uint8_t>,
                    media::VideoEncoderInfo::kMaxSpatialLayers>
  fps_allocation(const media::VideoEncoderInfo& video_encoder_info) {
    return video_encoder_info.fps_allocation;
  }
  static const std::vector<media::ResolutionBitrateLimit>&
  resolution_bitrate_limits(const media::VideoEncoderInfo& video_encoder_info) {
    return video_encoder_info.resolution_bitrate_limits;
  }

  static bool Read(media::mojom::VideoEncoderInfoDataView data,
                   media::VideoEncoderInfo* out);
};

}  // namespace mojo

#endif  // MEDIA_MOJO_MOJOM_VIDEO_ENCODER_INFO_MOJOM_TRAITS_H_
