// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "starboard/shared/starboard/media/cenc_utils.h"

#include <memory>
#include <vector>

#include "starboard/common/log.h"
#include "starboard/shared/starboard/media/mp4/box_definitions.h"
#include "starboard/shared/starboard/media/mp4/box_reader.h"

// Copied from media/cdm/cenc_utils.cc, reduced to ValidatePsshInput() and
// ported to C++17 without //base and //media.

namespace starboard {

// The initialization data for encrypted media files using the ISO Common
// Encryption ('cenc') protection scheme may contain one or more protection
// system specific header ('pssh') boxes.
// ref: https://www.w3.org/TR/eme-initdata-cenc/

// Returns true if |input| contains only 1 or more valid 'pssh' boxes, false
// otherwise. |pssh_boxes| is updated as the set of parsed 'pssh' boxes.
// Note: All boxes in |input| must be 'pssh' boxes. However, if they can't be
//       properly parsed (e.g. unsupported version), then they will be skipped.
static bool ReadAllPsshBoxes(
    span<const uint8_t> input,
    std::vector<FullProtectionSystemSpecificHeader>* pssh_boxes) {
  SB_CHECK(!input.empty());

  // Verify that |input| contains only 'pssh' boxes.
  // ReadAllChildrenAndCheckFourCC() is templated, so it checks that each
  // box in |input| matches the box type of the parameter (in this case
  // ProtectionSystemSpecificHeader is a 'pssh' box).
  // ProtectionSystemSpecificHeader doesn't validate the 'pssh' contents,
  // so this simply verifies that |input| only contains 'pssh' boxes and
  // nothing else.
  std::unique_ptr<BoxReader> input_reader(
      BoxReader::ReadConcatentatedBoxes(input));
  std::vector<ProtectionSystemSpecificHeader> raw_pssh_boxes;
  if (!input_reader->ReadAllChildrenAndCheckFourCC(&raw_pssh_boxes)) {
    return false;
  }

  // Now that we have |input| parsed into |raw_pssh_boxes|, reparse each one
  // into a FullProtectionSystemSpecificHeader, which extracts all the
  // relevant fields from the box. Since there may be unparsable 'pssh' boxes
  // (due to unsupported version, for example), this is done one by one,
  // ignoring any boxes that can't be parsed.
  for (const auto& raw_pssh_box : raw_pssh_boxes) {
    std::unique_ptr<BoxReader> raw_pssh_reader(
        BoxReader::ReadConcatentatedBoxes(raw_pssh_box.raw_box));
    // ReadAllChildren() appends any successfully parsed box onto it's
    // parameter, so |pssh_boxes| will contain the collection of successfully
    // parsed 'pssh' boxes. If an error occurs, try the next box.
    if (!raw_pssh_reader->ReadAllChildrenAndCheckFourCC(pssh_boxes)) {
      continue;
    }
  }

  // Must have successfully parsed at least one 'pssh' box.
  return pssh_boxes->size() > 0;
}

bool ValidatePsshInput(span<const uint8_t> input) {
  // No 'pssh' boxes is considered valid.
  if (input.empty()) {
    return true;
  }

  std::vector<FullProtectionSystemSpecificHeader> children;
  return ReadAllPsshBoxes(input, &children);
}

}  // namespace starboard
