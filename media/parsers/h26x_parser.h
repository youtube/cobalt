// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_PARSERS_H26X_PARSER_H_
#define MEDIA_PARSERS_H26X_PARSER_H_

#include <stdint.h>

#include <array>

#include "base/containers/heap_array.h"
#include "media/base/media_export.h"
#include "third_party/skia/include/private/SkHdrMetadata.h"

namespace media {

struct MEDIA_EXPORT H26xSEIContentLightLevelInfo {
  uint16_t max_content_light_level = 0;
  uint16_t max_picture_average_light_level = 0;

  skhdr::ContentLightLevelInformation ToSkHdr() const;
};

struct MEDIA_EXPORT H26xSEIMasteringDisplayInfo {
  enum {
    kNumDisplayPrimaries = 3,
    kDisplayPrimaryComponents = 2,
  };

  std::array<std::array<uint16_t, kDisplayPrimaryComponents>,
             kNumDisplayPrimaries>
      display_primaries = {};
  std::array<uint16_t, 2> white_points = {};
  uint32_t max_luminance = 0;
  uint32_t min_luminance = 0;

  skhdr::MasteringDisplayColorVolume ToSkHdr() const;
};

struct MEDIA_EXPORT H26xSEIUserDataRegisteredT35 {
  H26xSEIUserDataRegisteredT35();
  ~H26xSEIUserDataRegisteredT35();
  H26xSEIUserDataRegisteredT35(H26xSEIUserDataRegisteredT35&&);
  H26xSEIUserDataRegisteredT35& operator=(H26xSEIUserDataRegisteredT35&&);

  uint8_t country_code = 0;
  uint8_t country_code_extension_byte = 0;
  base::HeapArray<uint8_t> payload;
};

}  // namespace media

#endif  // MEDIA_PARSERS_H26X_PARSER_H_
