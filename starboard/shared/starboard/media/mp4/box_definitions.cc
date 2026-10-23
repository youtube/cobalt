// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "starboard/shared/starboard/media/mp4/box_definitions.h"

#include "starboard/common/span.h"

// Copied from media/formats/mp4/box_definitions.cc, reduced to the 'pssh'
// boxes and ported to C++17 without //base and //media.

namespace starboard::mp4 {

ProtectionSystemSpecificHeader::ProtectionSystemSpecificHeader() = default;
ProtectionSystemSpecificHeader::ProtectionSystemSpecificHeader(
    const ProtectionSystemSpecificHeader& other) = default;
ProtectionSystemSpecificHeader::~ProtectionSystemSpecificHeader() = default;
FourCC ProtectionSystemSpecificHeader::BoxType() const {
  return FOURCC_PSSH;
}

bool ProtectionSystemSpecificHeader::Parse(BoxReader* reader) {
  // Don't bother validating the box's contents.
  // Copy the entire box, including the header, for passing to EME as initData.
  SB_DCHECK(raw_box.empty());
  span<const uint8_t> buffer = reader->buffer().first(reader->box_size());
  raw_box.assign(buffer.begin(), buffer.end());
  return true;
}

FullProtectionSystemSpecificHeader::FullProtectionSystemSpecificHeader() =
    default;
FullProtectionSystemSpecificHeader::FullProtectionSystemSpecificHeader(
    const FullProtectionSystemSpecificHeader& other) = default;
FullProtectionSystemSpecificHeader::~FullProtectionSystemSpecificHeader() =
    default;
FourCC FullProtectionSystemSpecificHeader::BoxType() const {
  return FOURCC_PSSH;
}

// The format of a 'pssh' box is as follows:
//   unsigned int(32) size;
//   unsigned int(32) type = "pssh";
//   if (size==1) {
//     unsigned int(64) largesize;
//   } else if (size==0) {
//     -- box extends to end of file
//   }
//   unsigned int(8) version;
//   bit(24) flags;
//   unsigned int(8)[16] SystemID;
//   if (version > 0)
//   {
//     unsigned int(32) KID_count;
//     {
//       unsigned int(8)[16] KID;
//     } [KID_count]
//   }
//   unsigned int(32) DataSize;
//   unsigned int(8)[DataSize] Data;

bool FullProtectionSystemSpecificHeader::Parse(BoxReader* reader) {
  RCHECK(reader->type() == BoxType() && reader->ReadFullBoxHeader());

  // Only versions 0 and 1 of the 'pssh' boxes are supported. Any other
  // versions are ignored.
  RCHECK(reader->version() == 0 || reader->version() == 1);
  RCHECK(reader->flags() == 0);
  RCHECK(reader->ReadVec(&system_id, 16));

  if (reader->version() > 0) {
    uint32_t kid_count;
    RCHECK(reader->Read4(&kid_count));
    for (uint32_t i = 0; i < kid_count; ++i) {
      std::vector<uint8_t> kid;
      RCHECK(reader->ReadVec(&kid, 16));
      key_ids.push_back(kid);
    }
  }

  uint32_t data_size;
  RCHECK(reader->Read4(&data_size));
  RCHECK(reader->ReadVec(&data, data_size));
  return true;
}

}  // namespace starboard::mp4
