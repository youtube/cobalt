// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/state_machines/text_segmentation_machine_state.h"

#include <ostream>  // NOLINT
#include "base/check_op.h"

namespace blink {

std::ostream& operator<<(std::ostream& os, TextSegmentationMachineState state) {
  static const char* const kTexts[] = {
      "Invalid", "NeedMoreCodeUnit", "NeedFollowingCodeUnit", "Finished",
  };

  auto* const* const it = std::begin(kTexts) + static_cast<size_t>(state);
  DCHECK_GE(it, std::begin(kTexts)) << "Unknown state value";
  DCHECK_LT(it, std::end(kTexts)) << "Unknown state value";
  return os << *it;
}

}  // namespace blink
