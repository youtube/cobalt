// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/parsers/h26x_parser.h"

namespace media {

skhdr::ContentLightLevelInformation H26xSEIContentLightLevelInfo::ToSkHdr()
    const {
  return skhdr::ContentLightLevelInformation::MakeUint16(
      /*maxCLL=*/max_content_light_level,
      /*maxFALL=*/max_picture_average_light_level);
}

skhdr::MasteringDisplayColorVolume H26xSEIMasteringDisplayInfo::ToSkHdr()
    const {
  constexpr auto kChromaDenominator = 50000.0f;
  constexpr auto kLumaDenominator = 10000.0f;
  // display primaries are in G/B/R order in MDCV SEI.
  return {
      .fDisplayPrimaries = {display_primaries[2][0] / kChromaDenominator,
                            display_primaries[2][1] / kChromaDenominator,
                            display_primaries[0][0] / kChromaDenominator,
                            display_primaries[0][1] / kChromaDenominator,
                            display_primaries[1][0] / kChromaDenominator,
                            display_primaries[1][1] / kChromaDenominator,
                            white_points[0] / kChromaDenominator,
                            white_points[1] / kChromaDenominator},
      .fMaximumDisplayMasteringLuminance = max_luminance / kLumaDenominator,
      .fMinimumDisplayMasteringLuminance = min_luminance / kLumaDenominator};
}

H26xSEIUserDataRegisteredT35::H26xSEIUserDataRegisteredT35() = default;
H26xSEIUserDataRegisteredT35::~H26xSEIUserDataRegisteredT35() = default;
H26xSEIUserDataRegisteredT35::H26xSEIUserDataRegisteredT35(
    H26xSEIUserDataRegisteredT35&&) = default;
H26xSEIUserDataRegisteredT35& H26xSEIUserDataRegisteredT35::operator=(
    H26xSEIUserDataRegisteredT35&&) = default;

}  // namespace media
