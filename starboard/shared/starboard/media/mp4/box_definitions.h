// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STARBOARD_SHARED_STARBOARD_MEDIA_MP4_BOX_DEFINITIONS_H_
#define STARBOARD_SHARED_STARBOARD_MEDIA_MP4_BOX_DEFINITIONS_H_

#include <stdint.h>

#include <vector>

#include "starboard/shared/starboard/media/mp4/box_reader.h"

// Copied from media/formats/mp4/box_definitions.h, reduced to the 'pssh' boxes
// and ported to C++17 without //base and //media.

namespace starboard::mp4 {

#define DECLARE_BOX_METHODS(T)            \
  T();                                    \
  T(const T& other);                      \
  ~T() override;                          \
  bool Parse(BoxReader* reader) override; \
  FourCC BoxType() const override

// If only copying the 'pssh' boxes, use ProtectionSystemSpecificHeader.
// If access to the individual fields is needed, use
// FullProtectionSystemSpecificHeader.
struct ProtectionSystemSpecificHeader : Box {
  DECLARE_BOX_METHODS(ProtectionSystemSpecificHeader);

  std::vector<uint8_t> raw_box;
};

struct FullProtectionSystemSpecificHeader : Box {
  DECLARE_BOX_METHODS(FullProtectionSystemSpecificHeader);

  std::vector<uint8_t> system_id;
  std::vector<std::vector<uint8_t>> key_ids;
  std::vector<uint8_t> data;
};

#undef DECLARE_BOX_METHODS

}  // namespace starboard::mp4

#endif  // STARBOARD_SHARED_STARBOARD_MEDIA_MP4_BOX_DEFINITIONS_H_
