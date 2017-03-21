// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/media/base/media_content_type.h"

namespace media {

namespace {
const int kMinimumContentDurationSecs = 5;
}  // anonymous namespace

MediaContentType DurationToMediaContentType(base::TimeDelta duration) {
  // A zero duration indicates that the duration is unknown. "Persistent" type
  // should be used in this case.
  return (duration.is_zero() ||
          duration > base::TimeDelta::FromSeconds(kMinimumContentDurationSecs))
             ? MediaContentType::Persistent
             : MediaContentType::Transient;
}

}  // namespace media
