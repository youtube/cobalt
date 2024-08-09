// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_MOJO_MOJOM_VIDEO_DECODER_CONFIG_MOJOM_TRAITS_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_MOJO_MOJOM_VIDEO_DECODER_CONFIG_MOJOM_TRAITS_H_

#include "third_party/chromium/media/base/ipc/media_param_traits.h"
#include "third_party/chromium/media/base/video_decoder_config.h"
#include "third_party/chromium/media/mojo/mojom/media_types.mojom.h"
#include "third_party/chromium/media/mojo/mojom/video_color_space_mojom_traits.h"
#include "third_party/chromium/media/mojo/mojom/video_transformation_mojom_traits.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"
#include "ui/gfx/mojom/hdr_metadata_mojom_traits.h"

namespace mojo {

template <>
struct StructTraits<media_m96::mojom::VideoDecoderConfigDataView,
                    media_m96::VideoDecoderConfig> {
  static media_m96::VideoCodec codec(const media_m96::VideoDecoderConfig& input) {
    return input.codec();
  }

  static media_m96::VideoCodecProfile profile(
      const media_m96::VideoDecoderConfig& input) {
    return input.profile();
  }

  static bool has_alpha(const media_m96::VideoDecoderConfig& input) {
    return input.alpha_mode() ==
           media_m96::VideoDecoderConfig::AlphaMode::kHasAlpha;
  }

  static const gfx::Size& coded_size(const media_m96::VideoDecoderConfig& input) {
    return input.coded_size();
  }

  static const gfx::Rect& visible_rect(const media_m96::VideoDecoderConfig& input) {
    return input.visible_rect();
  }

  static const gfx::Size& natural_size(const media_m96::VideoDecoderConfig& input) {
    return input.natural_size();
  }

  static const std::vector<uint8_t>& extra_data(
      const media_m96::VideoDecoderConfig& input) {
    return input.extra_data();
  }

  static media_m96::EncryptionScheme encryption_scheme(
      const media_m96::VideoDecoderConfig& input) {
    return input.encryption_scheme();
  }

  static const media_m96::VideoColorSpace& color_space_info(
      const media_m96::VideoDecoderConfig& input) {
    return input.color_space_info();
  }

  static media_m96::VideoTransformation transformation(
      const media_m96::VideoDecoderConfig& input) {
    return input.video_transformation();
  }

  static const absl::optional<gfx::HDRMetadata>& hdr_metadata(
      const media_m96::VideoDecoderConfig& input) {
    return input.hdr_metadata();
  }

  static uint32_t level(const media_m96::VideoDecoderConfig& input) {
    return input.level();
  }

  static bool Read(media_m96::mojom::VideoDecoderConfigDataView input,
                   media_m96::VideoDecoderConfig* output);
};

}  // namespace mojo

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_MOJO_MOJOM_VIDEO_DECODER_CONFIG_MOJOM_TRAITS_H_
