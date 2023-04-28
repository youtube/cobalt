// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_URL_CONSTANTS_H_
#define COMPONENTS_METRICS_URL_CONSTANTS_H_

namespace metrics {

// The new metrics server's URL.
extern const char kNewMetricsServerUrl[];

// The HTTP fallback metrics server's URL.
extern const char kNewMetricsServerUrlInsecure[];

// The old metrics server's URL.
extern const char kOldMetricsServerUrl[];

// The default MIME type for the uploaded metrics data.
extern const char kDefaultMetricsMimeType[];

} // namespace metrics

#endif // COMPONENTS_METRICS_URL_CONSTANTS_H_
