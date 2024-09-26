// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_STRUCTURED_EVENT_LOGGING_FEATURES_H_
#define CHROME_BROWSER_METRICS_STRUCTURED_EVENT_LOGGING_FEATURES_H_

#include "base/feature_list.h"

namespace metrics::structured {

// Controls whether app discovery logging is enabled or not.
BASE_DECLARE_FEATURE(kAppDiscoveryLogging);

}  // namespace metrics::structured

#endif  // CHROME_BROWSER_METRICS_STRUCTURED_EVENT_LOGGING_FEATURES_H_
