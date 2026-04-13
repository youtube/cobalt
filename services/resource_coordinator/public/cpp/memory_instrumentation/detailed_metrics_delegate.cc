// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/public/cpp/memory_instrumentation/detailed_metrics_delegate.h"

namespace memory_instrumentation {

DetailedMetrics::DetailedMetrics() = default;
DetailedMetrics::~DetailedMetrics() = default;
DetailedMetrics::DetailedMetrics(DetailedMetrics&&) = default;
DetailedMetrics& DetailedMetrics::operator=(DetailedMetrics&&) = default;

}  // namespace memory_instrumentation
