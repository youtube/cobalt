// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_HDR_METADATA_H_
#define UI_GFX_HDR_METADATA_H_

#include "media/base/media_export.h"
#include "ui/gfx/geometry/point_f.h"

namespace gfx {

// SMPTE ST 2086 color volume metadata.
struct MEDIA_EXPORT ColorVolumeMetadata {
  using Chromaticity = gfx::PointF;
  Chromaticity primary_r;
  Chromaticity primary_g;
  Chromaticity primary_b;
  Chromaticity white_point;
  float luminance_max = 0;
  float luminance_min = 0;

  ColorVolumeMetadata() = default;
  ColorVolumeMetadata(const ColorVolumeMetadata& rhs) = default;
  ColorVolumeMetadata& operator=(const ColorVolumeMetadata& rhs) = default;

  bool operator==(const ColorVolumeMetadata& rhs) const {
    return ((primary_r == rhs.primary_r) && (primary_g == rhs.primary_g) &&
            (primary_b == rhs.primary_b) && (white_point == rhs.white_point) &&
            (luminance_max == rhs.luminance_max) &&
            (luminance_min == rhs.luminance_min));
  }
};

// HDR metadata common for HDR10 and WebM/VP9-based HDR formats.
struct MEDIA_EXPORT HDRMetadata {
  ColorVolumeMetadata color_volume_metadata;
  // Max content light level (CLL), i.e. maximum brightness level present in the
  // stream), in nits.
  unsigned max_content_light_level = 0;
  // Max frame-average light level (FALL), i.e. maximum average brightness of
  // the brightest frame in the stream), in nits.
  unsigned max_frame_average_light_level = 0;

  HDRMetadata() = default;
  HDRMetadata(const HDRMetadata& rhs) = default;
  HDRMetadata& operator=(const HDRMetadata& rhs) = default;

  bool IsValid() const {
    return !((max_content_light_level == 0) &&
             (max_frame_average_light_level == 0) &&
             (color_volume_metadata == ColorVolumeMetadata()));
  }

  bool operator==(const HDRMetadata& rhs) const {
    return (
        (max_content_light_level == rhs.max_content_light_level) &&
        (max_frame_average_light_level == rhs.max_frame_average_light_level) &&
        (color_volume_metadata == rhs.color_volume_metadata));
  }

  bool operator!=(const HDRMetadata& rhs) const { return !(*this == rhs); }
};

// HDR metadata types as described in
// https://w3c.github.io/media-capabilities/#enumdef-hdrmetadatatype
enum class HdrMetadataType {
  kNone,
  kSmpteSt2086,
  kSmpteSt2094_10,
  kSmpteSt2094_40,
};

}  // namespace gfx

#endif  // UI_GFX_HDR_METADATA_H_
