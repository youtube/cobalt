// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_MOJO_MOJOM_SUPPORTED_VIDEO_DECODER_CONFIG_MOJOM_TRAITS_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_MOJO_MOJOM_SUPPORTED_VIDEO_DECODER_CONFIG_MOJOM_TRAITS_H_

#include "third_party/chromium/media/base/ipc/media_param_traits.h"
#include "third_party/chromium/media/base/supported_video_decoder_config.h"
#include "third_party/chromium/media/mojo/mojom/media_types.mojom.h"
#include "third_party/chromium/media/mojo/mojom/video_decoder.mojom.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"

namespace mojo {

template <>
struct StructTraits<media_m96::mojom::SupportedVideoDecoderConfigDataView,
                    media_m96::SupportedVideoDecoderConfig> {
  static media_m96::VideoCodecProfile profile_min(
      const media_m96::SupportedVideoDecoderConfig& input) {
    return input.profile_min;
  }

  static media_m96::VideoCodecProfile profile_max(
      const media_m96::SupportedVideoDecoderConfig& input) {
    return input.profile_max;
  }

  static const gfx::Size& coded_size_min(
      const media_m96::SupportedVideoDecoderConfig& input) {
    return input.coded_size_min;
  }

  static const gfx::Size& coded_size_max(
      const media_m96::SupportedVideoDecoderConfig& input) {
    return input.coded_size_max;
  }

  static bool allow_encrypted(const media_m96::SupportedVideoDecoderConfig& input) {
    return input.allow_encrypted;
  }

  static bool require_encrypted(
      const media_m96::SupportedVideoDecoderConfig& input) {
    return input.require_encrypted;
  }

  static bool Read(media_m96::mojom::SupportedVideoDecoderConfigDataView input,
                   media_m96::SupportedVideoDecoderConfig* output);
};

}  // namespace mojo

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_MOJO_MOJOM_SUPPORTED_VIDEO_DECODER_CONFIG_MOJOM_TRAITS_H_
