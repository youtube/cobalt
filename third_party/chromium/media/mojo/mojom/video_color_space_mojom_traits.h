// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_MOJO_MOJOM_VIDEO_COLOR_SPACE_MOJOM_TRAITS_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_MOJO_MOJOM_VIDEO_COLOR_SPACE_MOJOM_TRAITS_H_

#include "third_party/chromium/media/base/video_color_space.h"
#include "third_party/chromium/media/mojo/mojom/media_types.mojom.h"

namespace mojo {

template <>
struct StructTraits<media_m96::mojom::VideoColorSpaceDataView,
                    media_m96::VideoColorSpace> {
  static media_m96::VideoColorSpace::PrimaryID primaries(
      const media_m96::VideoColorSpace& input) {
    return input.primaries;
  }
  static media_m96::VideoColorSpace::TransferID transfer(
      const media_m96::VideoColorSpace& input) {
    return input.transfer;
  }
  static media_m96::VideoColorSpace::MatrixID matrix(
      const media_m96::VideoColorSpace& input) {
    return input.matrix;
  }
  static gfx::ColorSpace::RangeID range(const media_m96::VideoColorSpace& input) {
    return input.range;
  }

  static bool Read(media_m96::mojom::VideoColorSpaceDataView data,
                   media_m96::VideoColorSpace* output) {
    output->primaries =
        static_cast<media_m96::VideoColorSpace::PrimaryID>(data.primaries());
    output->transfer =
        static_cast<media_m96::VideoColorSpace::TransferID>(data.transfer());
    output->matrix =
        static_cast<media_m96::VideoColorSpace::MatrixID>(data.matrix());
    output->range = static_cast<gfx::ColorSpace::RangeID>(data.range());
    return true;
  }
};

}  // namespace mojo

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_MOJO_MOJOM_VIDEO_COLOR_SPACE_MOJOM_TRAITS_H_
