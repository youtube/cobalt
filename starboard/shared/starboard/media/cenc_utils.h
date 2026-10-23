// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STARBOARD_SHARED_STARBOARD_MEDIA_CENC_UTILS_H_
#define STARBOARD_SHARED_STARBOARD_MEDIA_CENC_UTILS_H_

#include <stdint.h>

#include "starboard/common/span.h"

// Copied from media/cdm/cenc_utils.h, reduced to ValidatePsshInput() and
// ported to C++17 without //base and //media.

namespace starboard {

// Validate that `input` is a set of concatenated 'pssh' boxes and the sizes
// match. Returns true if `input` looks valid, false otherwise.
bool ValidatePsshInput(span<const uint8_t> input);

}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_MEDIA_CENC_UTILS_H_
