// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/qpack/qpack_test_utils.h"
#if defined(STARBOARD)
#include "base/logging.h"
#endif

#include <limits>

namespace quic {
namespace test {

FragmentSizeGenerator FragmentModeToFragmentSizeGenerator(
    FragmentMode fragment_mode) {
  switch (fragment_mode) {
    case FragmentMode::kSingleChunk:
      return []() { return std::numeric_limits<size_t>::max(); };
    case FragmentMode::kOctetByOctet:
      return []() { return 1; };
#if defined(STARBOARD)
    // Some compilers doesn't allow missing return path.
    default:
      NOTREACHED();
      return []() { return std::numeric_limits<size_t>::max(); };
#endif
  }
}

}  // namespace test
}  // namespace quic
