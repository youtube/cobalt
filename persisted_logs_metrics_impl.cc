// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/persisted_logs_metrics_impl.h"

#include "base/metrics/histogram_macros.h"

namespace ukm {

PersistedLogsMetricsImpl::PersistedLogsMetricsImpl() = default;

PersistedLogsMetricsImpl::~PersistedLogsMetricsImpl() = default;

void PersistedLogsMetricsImpl::RecordLogReadStatus(
    metrics::PersistedLogsMetrics::LogReadStatus status) {
  UMA_HISTOGRAM_ENUMERATION("UKM.PersistentLogRecall.Status", status,
                            metrics::PersistedLogsMetrics::END_RECALL_STATUS);
}

void PersistedLogsMetricsImpl::RecordCompressionRatio(size_t compressed_size,
                                                      size_t original_size) {
  UMA_HISTOGRAM_PERCENTAGE(
      "UKM.ProtoCompressionRatio",
      static_cast<int>(100 * compressed_size / original_size));
}

void PersistedLogsMetricsImpl::RecordDroppedLogSize(size_t size) {
  UMA_HISTOGRAM_COUNTS_1M("UKM.UnsentLogs.DroppedSize", static_cast<int>(size));
}

void PersistedLogsMetricsImpl::RecordDroppedLogsNum(int dropped_logs_num) {
  UMA_HISTOGRAM_COUNTS_10000("UKM.UnsentLogs.NumDropped", dropped_logs_num);
}

}  // namespace ukm
