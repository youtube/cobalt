// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_MOJO_MOJOM_VIDEO_ENCODER_INFO_MOJOM_TRAITS_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_MOJO_MOJOM_VIDEO_ENCODER_INFO_MOJOM_TRAITS_H_

#include "base/containers/span.h"
#include "third_party/chromium/media/mojo/mojom/video_encoder_info.mojom-shared.h"
#include "third_party/chromium/media/video/video_encoder_info.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"

namespace mojo {

template <>
class StructTraits<media_m96::mojom::ResolutionBitrateLimitDataView,
                   media_m96::ResolutionBitrateLimit> {
 public:
  static const gfx::Size& frame_size(
      const media_m96::ResolutionBitrateLimit& resolution_bitrate_limit) {
    return resolution_bitrate_limit.frame_size;
  }
  static int min_start_bitrate_bps(
      const media_m96::ResolutionBitrateLimit& resolution_bitrate_limit) {
    return resolution_bitrate_limit.min_start_bitrate_bps;
  }
  static int min_bitrate_bps(
      const media_m96::ResolutionBitrateLimit& resolution_bitrate_limit) {
    return resolution_bitrate_limit.min_bitrate_bps;
  }
  static int max_bitrate_bps(
      const media_m96::ResolutionBitrateLimit& resolution_bitrate_limit) {
    return resolution_bitrate_limit.max_bitrate_bps;
  }
  static bool Read(media_m96::mojom::ResolutionBitrateLimitDataView data,
                   media_m96::ResolutionBitrateLimit* out);
};

template <>
class StructTraits<media_m96::mojom::VideoEncoderInfoDataView,
                   media_m96::VideoEncoderInfo> {
 public:
  static const std::string& implementation_name(
      const media_m96::VideoEncoderInfo& video_encoder_info) {
    return video_encoder_info.implementation_name;
  }
  static bool supports_native_handle(
      const media_m96::VideoEncoderInfo& video_encoder_info) {
    return video_encoder_info.supports_native_handle;
  }
  static bool has_trusted_rate_controller(
      const media_m96::VideoEncoderInfo& video_encoder_info) {
    return video_encoder_info.has_trusted_rate_controller;
  }
  static bool is_hardware_accelerated(
      const media_m96::VideoEncoderInfo& video_encoder_info) {
    return video_encoder_info.is_hardware_accelerated;
  }
  static bool supports_simulcast(
      const media_m96::VideoEncoderInfo& video_encoder_info) {
    return video_encoder_info.supports_simulcast;
  }
  static base::span<const std::vector<uint8_t>,
                    media_m96::VideoEncoderInfo::kMaxSpatialLayers>
  fps_allocation(const media_m96::VideoEncoderInfo& video_encoder_info) {
    return video_encoder_info.fps_allocation;
  }
  static const std::vector<media_m96::ResolutionBitrateLimit>&
  resolution_bitrate_limits(const media_m96::VideoEncoderInfo& video_encoder_info) {
    return video_encoder_info.resolution_bitrate_limits;
  }

  static bool Read(media_m96::mojom::VideoEncoderInfoDataView data,
                   media_m96::VideoEncoderInfo* out);
};

}  // namespace mojo

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_MOJO_MOJOM_VIDEO_ENCODER_INFO_MOJOM_TRAITS_H_
