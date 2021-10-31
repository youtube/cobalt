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

// Module Overview: Starboard Accessibility API

// Provide helper macros and functions useful to the implementations of the
// Starboard Accessibility API on all platforms

#ifndef STARBOARD_SHARED_STARBOARD_ACCESSIBILITY_INTERNAL_H_
#define STARBOARD_SHARED_STARBOARD_ACCESSIBILITY_INTERNAL_H_

#include <limits>

#include "starboard/accessibility.h"
#include "starboard/common/log.h"

namespace starboard {
namespace shared {
namespace starboard {

const int kRgbWhite = 0xFFFFFF;
const int kRgbBlack = 0x000000;
const int kRgbRed = 0xFF0000;
const int kRgbYellow = 0xFFFF00;
const int kRgbGreen = 0x00FF00;
const int kRgbCyan = 0x00FFFF;
const int kRgbBlue = 0x0000FF;
const int kRgbMagenta = 0xFF00FF;

const int kRgbColors[] = {
    kRgbWhite, kRgbBlack, kRgbRed,  kRgbYellow,
    kRgbGreen, kRgbCyan,  kRgbBlue, kRgbMagenta,
};

SbAccessibilityCaptionColor GetClosestCaptionColorRGB(int r, int g, int b) {
  int ref_color = kRgbWhite;
  int min_distance = std::numeric_limits<int>::max();

  // Find the reference color with the least distance (squared).
  for (int i = 0; i < SB_ARRAY_SIZE(kRgbColors); i++) {
    int r_ref = 0xFF & (kRgbColors[i] >> 16);
    int g_ref = 0xFF & (kRgbColors[i] >> 8);
    int b_ref = 0xFF & (kRgbColors[i]);
    int distance_squared =
        pow(r - r_ref, 2) + pow(g - g_ref, 2) + pow(b - b_ref, 2);
    if (distance_squared < min_distance) {
      ref_color = kRgbColors[i];
      min_distance = distance_squared;
    }
  }

  switch (ref_color) {
    case kRgbWhite:
      return kSbAccessibilityCaptionColorWhite;
    case kRgbBlack:
      return kSbAccessibilityCaptionColorBlack;
    case kRgbRed:
      return kSbAccessibilityCaptionColorRed;
    case kRgbYellow:
      return kSbAccessibilityCaptionColorYellow;
    case kRgbGreen:
      return kSbAccessibilityCaptionColorGreen;
    case kRgbCyan:
      return kSbAccessibilityCaptionColorCyan;
    case kRgbBlue:
      return kSbAccessibilityCaptionColorBlue;
    case kRgbMagenta:
      return kSbAccessibilityCaptionColorMagenta;
    default:
      SB_NOTREACHED() << "Invalid RGB color conversion";
      return kSbAccessibilityCaptionColorWhite;
  }
}

SbAccessibilityCaptionColor GetClosestCaptionColor(int color) {
  int r = 0xFF & (color >> 16);
  int g = 0xFF & (color >> 8);
  int b = 0xFF & (color);
  return GetClosestCaptionColorRGB(r, g, b);
}

int FindClosestReferenceValue(int value,
                              const int reference[],
                              size_t reference_size) {
  int result = reference[0];
  int min_difference = std::numeric_limits<int>::max();

  for (int i = 0; i < reference_size; i++) {
    int difference = abs(reference[i] - value);
    if (difference < min_difference) {
      result = reference[i];
      min_difference = difference;
    }
  }
  return result;
}

const int kFontSizes[] = {25,  50,  75,  100, 125, 150,
                          175, 200, 225, 250, 275, 300};

SbAccessibilityCaptionFontSizePercentage GetClosestFontSizePercentage(
    int font_size_percent) {
  int reference_size = FindClosestReferenceValue(font_size_percent, kFontSizes,
                                                 SB_ARRAY_SIZE(kFontSizes));
  switch (reference_size) {
    case 25:
      return kSbAccessibilityCaptionFontSizePercentage25;
    case 50:
      return kSbAccessibilityCaptionFontSizePercentage50;
    case 75:
      return kSbAccessibilityCaptionFontSizePercentage75;
    case 100:
      return kSbAccessibilityCaptionFontSizePercentage100;
    case 125:
      return kSbAccessibilityCaptionFontSizePercentage125;
    case 150:
      return kSbAccessibilityCaptionFontSizePercentage150;
    case 175:
      return kSbAccessibilityCaptionFontSizePercentage175;
    case 200:
      return kSbAccessibilityCaptionFontSizePercentage200;
    case 225:
      return kSbAccessibilityCaptionFontSizePercentage225;
    case 250:
      return kSbAccessibilityCaptionFontSizePercentage250;
    case 275:
      return kSbAccessibilityCaptionFontSizePercentage275;
    case 300:
      return kSbAccessibilityCaptionFontSizePercentage300;
    default:
      SB_NOTREACHED() << "Invalid font size";
      return kSbAccessibilityCaptionFontSizePercentage100;
  }
}

const int kOpacities[] = {
    0, 25, 50, 75, 100,
};

SbAccessibilityCaptionOpacityPercentage GetClosestOpacity(int opacity_percent) {
  int reference_opacity_percent = FindClosestReferenceValue(
      opacity_percent, kOpacities, SB_ARRAY_SIZE(kOpacities));
  switch (reference_opacity_percent) {
    case 0:
      return kSbAccessibilityCaptionOpacityPercentage0;
    case 25:
      return kSbAccessibilityCaptionOpacityPercentage25;
    case 50:
      return kSbAccessibilityCaptionOpacityPercentage50;
    case 75:
      return kSbAccessibilityCaptionOpacityPercentage75;
    case 100:
      return kSbAccessibilityCaptionOpacityPercentage100;
    default:
      SB_NOTREACHED() << "Invalid opacity percentage";
      return kSbAccessibilityCaptionOpacityPercentage100;
  }
}

}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_ACCESSIBILITY_INTERNAL_H_
