// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_BASE_TIMESTAMP_CONSTANTS_H_
#define COBALT_MEDIA_BASE_TIMESTAMP_CONSTANTS_H_

#include <limits>

#include "base/time/time.h"
#include "starboard/types.h"

namespace cobalt {
namespace media {

// Indicates an invalid or missing timestamp.
const base::TimeDelta kNoTimestamp =
    base::TimeDelta::FromMicroseconds(std::numeric_limits<int64_t>::min());

// Represents an infinite stream duration.
const base::TimeDelta kInfiniteDuration =
    base::TimeDelta::FromMicroseconds(std::numeric_limits<int64_t>::max());

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_BASE_TIMESTAMP_CONSTANTS_H_
