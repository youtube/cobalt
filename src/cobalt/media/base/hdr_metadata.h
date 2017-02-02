// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_BASE_HDR_METADATA_H_
#define COBALT_MEDIA_BASE_HDR_METADATA_H_

#include "cobalt/media/base/media_export.h"

namespace media {

// SMPTE ST 2086 mastering metadata.
struct MEDIA_EXPORT MasteringMetadata {
  float primary_r_chromaticity_x;
  float primary_r_chromaticity_y;
  float primary_g_chromaticity_x;
  float primary_g_chromaticity_y;
  float primary_b_chromaticity_x;
  float primary_b_chromaticity_y;
  float white_point_chromaticity_x;
  float white_point_chromaticity_y;
  float luminance_max;
  float luminance_min;

  MasteringMetadata();
  MasteringMetadata(const MasteringMetadata& rhs);
};

// HDR metadata common for HDR10 and WebM/VP9-based HDR formats.
struct MEDIA_EXPORT HDRMetadata {
  MasteringMetadata mastering_metadata;
  unsigned max_cll;
  unsigned max_fall;

  HDRMetadata();
  HDRMetadata(const HDRMetadata& rhs);
};

}  // namespace media

#endif  // COBALT_MEDIA_BASE_HDR_METADATA_H_
