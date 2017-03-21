// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/hdr_metadata.h"

namespace media {

MasteringMetadata::MasteringMetadata() {
  primary_r_chromaticity_x = 0;
  primary_r_chromaticity_y = 0;
  primary_g_chromaticity_x = 0;
  primary_g_chromaticity_y = 0;
  primary_b_chromaticity_x = 0;
  primary_b_chromaticity_y = 0;
  white_point_chromaticity_x = 0;
  white_point_chromaticity_y = 0;
  luminance_max = 0;
  luminance_min = 0;
}

HDRMetadata::HDRMetadata() {
  max_cll = 0;
  max_fall = 0;
}

}  // namespace media
