// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/internal/metrics.h"

#include "base/metrics/histogram_functions.h"

namespace collaboration::metrics {

void RecordJoinEvent(CollaborationServiceJoinEvent event) {
  base::UmaHistogramEnumeration("CollaborationService.JoinFlow", event);
}

void RecordShareOrManageEvent(CollaborationServiceShareOrManageEvent event) {
  base::UmaHistogramEnumeration("CollaborationService.ShareOrManageFlow",
                                event);
}

}  // namespace collaboration::metrics
