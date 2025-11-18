// Copyright 2020 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef STARBOARD_TVOS_SHARED_MEDIA_VP9_AV_SAMPLE_BUFFER_HELPER_H_
#define STARBOARD_TVOS_SHARED_MEDIA_VP9_AV_SAMPLE_BUFFER_HELPER_H_

#import <AVFoundation/AVFoundation.h>

#import "starboard/media.h"

namespace starboard {

const float kGammaLevel22 = 2.2f;
const float kGammaLevel24 = 2.4f;
const float kGammaLevel28 = 2.8f;

// LightLevelInfoSEI should in big endian.
struct LightLevelInfoSEI {
  uint16_t max_cll;
  uint16_t max_fall;

  LightLevelInfoSEI(const SbMediaColorMetadata& color)
      : max_cll(__builtin_bswap16(color.max_cll)),
        max_fall(__builtin_bswap16(color.max_fall)) {}
};
static_assert(sizeof(LightLevelInfoSEI) == 4,
              "Light level info must be 4 bytes");

// MasteringDisplayColorVolumeSEI should in big endian.
struct MasteringDisplayColorVolumeSEI {
  uint16_t green_point_x;
  uint16_t green_point_y;
  uint16_t blue_point_x;
  uint16_t blue_point_y;
  uint16_t red_point_x;
  uint16_t red_point_y;
  uint16_t white_point_x;
  uint16_t white_point_y;
  uint32_t max_luminance;
  uint32_t min_luminance;

  MasteringDisplayColorVolumeSEI(const SbMediaMasteringMetadata& metadata)
      : green_point_x(ConvertChromaticity(metadata.primary_g_chromaticity_x)),
        green_point_y(ConvertChromaticity(metadata.primary_g_chromaticity_y)),
        blue_point_x(ConvertChromaticity(metadata.primary_b_chromaticity_y)),
        blue_point_y(ConvertChromaticity(metadata.primary_b_chromaticity_y)),
        red_point_x(ConvertChromaticity(metadata.primary_r_chromaticity_x)),
        red_point_y(ConvertChromaticity(metadata.primary_r_chromaticity_y)),
        white_point_x(ConvertChromaticity(metadata.white_point_chromaticity_x)),
        white_point_y(ConvertChromaticity(metadata.white_point_chromaticity_y)),
        max_luminance(ConvertLuminance(metadata.luminance_max)),
        min_luminance(ConvertLuminance(metadata.luminance_min)) {}

 private:
  uint16_t ConvertChromaticity(float value) {
    return __builtin_bswap16(static_cast<uint16_t>(value * 50000.0f + 0.5f));
  }

  uint32_t ConvertLuminance(double value) {
    return __builtin_bswap32(static_cast<uint32_t>(value * 10000.0f + 0.5f));
  }
};
static_assert(sizeof(MasteringDisplayColorVolumeSEI) == 24,
              "Mastering meta data must be 24 bytes");

}  // namespace starboard

#endif  // STARBOARD_TVOS_SHARED_MEDIA_VP9_AV_SAMPLE_BUFFER_HELPER_H_
