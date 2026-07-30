// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_PRIVATE_METRICS_PRIVATE_INSIGHTS_CONTEXTUAL_CUE_LOG_EVENT_HELPERS_H_
#define COMPONENTS_METRICS_PRIVATE_METRICS_PRIVATE_INSIGHTS_CONTEXTUAL_CUE_LOG_EVENT_HELPERS_H_

#include <string>
#include <vector>

#include "components/metrics/private_metrics/private_insights/events/contextual_cue_log_event.pb.h"

namespace private_insights {

// Serializes a list of PageInfo protos to a JSON array string.
// Returns "[]" if serialization fails.
std::string SerializePageInfoListToJson(
    const std::vector<events::ContextualCueLogEvent::PageInfo>& list);

}  // namespace private_insights

#endif  // COMPONENTS_METRICS_PRIVATE_METRICS_PRIVATE_INSIGHTS_CONTEXTUAL_CUE_LOG_EVENT_HELPERS_H_
