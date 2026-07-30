// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/common/features.h"

#include "base/feature_list.h"
#include "build/build_config.h"

namespace page_load_metrics::features {

// Throttle sending custom user timings to the browser process.
// crbug.com/467177770 for more details.
BASE_FEATURE(kThrottleSendingCustomUserTimings,
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace page_load_metrics::features
