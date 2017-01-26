// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_HDR_METADATA_H_
#define MEDIA_BASE_HDR_METADATA_H_

#include "media/base/media_export.h"
#include "ui/gfx/color_space.h"

namespace media {

// SMPTE ST 2086 mastering metadata.
struct MEDIA_EXPORT MasteringMetadata {
  float primary_r_chromaticity_x = 0;
  float primary_r_chromaticity_y = 0;
  float primary_g_chromaticity_x = 0;
  float primary_g_chromaticity_y = 0;
  float primary_b_chromaticity_x = 0;
  float primary_b_chromaticity_y = 0;
  float white_point_chromaticity_x = 0;
  float white_point_chromaticity_y = 0;
  float luminance_max = 0;
  float luminance_min = 0;

  MasteringMetadata();
  MasteringMetadata(const MasteringMetadata& rhs);

  bool operator==(const MasteringMetadata& rhs) const {
    return ((primary_r_chromaticity_x == rhs.primary_r_chromaticity_x) &&
            (primary_r_chromaticity_y == rhs.primary_r_chromaticity_y) &&
            (primary_g_chromaticity_x == rhs.primary_g_chromaticity_x) &&
            (primary_g_chromaticity_y == rhs.primary_g_chromaticity_y) &&
            (primary_b_chromaticity_x == rhs.primary_b_chromaticity_x) &&
            (primary_b_chromaticity_y == rhs.primary_b_chromaticity_y) &&
            (white_point_chromaticity_x == rhs.white_point_chromaticity_x) &&
            (white_point_chromaticity_y == rhs.white_point_chromaticity_y) &&
            (luminance_max == rhs.luminance_max) &&
            (luminance_min == rhs.luminance_min));
  }
};

// HDR metadata common for HDR10 and WebM/VP9-based HDR formats.
struct MEDIA_EXPORT HDRMetadata {
  MasteringMetadata mastering_metadata;
  unsigned max_cll = 0;
  unsigned max_fall = 0;

  HDRMetadata();
  HDRMetadata(const HDRMetadata& rhs);

  bool operator==(const HDRMetadata& rhs) const {
    return ((max_cll == rhs.max_cll) && (max_fall == rhs.max_fall) &&
            (mastering_metadata == rhs.mastering_metadata));
  }
};

}  // namespace media

#endif  // MEDIA_BASE_HDR_METADATA_H_
