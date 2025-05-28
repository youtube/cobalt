// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ReportingService specialized to report DWA metrics.

#include "components/metrics/dwa/dwa_reporting_service.h"

#include <memory>
#include <string_view>

#include "components/metrics/dwa/dwa_pref_names.h"
#include "components/metrics/metrics_service_client.h"
#include "components/metrics/server_urls.h"
#include "components/metrics/unsent_log_store.h"
#include "components/metrics/unsent_log_store_metrics.h"
#include "components/prefs/pref_registry_simple.h"

namespace metrics::dwa {

DwaReportingService::DwaReportingService(
    metrics::MetricsServiceClient* client,
    PrefService* local_state,
    const UnsentLogStore::UnsentLogStoreLimits& storage_limits)
    : ReportingService(client,
                       local_state,
                       storage_limits.max_log_size_bytes,
                       /*logs_event_manager=*/nullptr),
      unsent_log_store_(std::make_unique<UnsentLogStoreMetrics>(),
                        local_state,
                        prefs::kUnsentLogStoreName,
                        /*metadata_pref_name=*/nullptr,
                        storage_limits,
                        client->GetUploadSigningKey(),
                        /*logs_event_manager=*/nullptr) {}

DwaReportingService::~DwaReportingService() = default;

metrics::UnsentLogStore* DwaReportingService::unsent_log_store() {
  return &unsent_log_store_;
}

// static
void DwaReportingService::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kUnsentLogStoreName);
}

metrics::LogStore* DwaReportingService::log_store() {
  return &unsent_log_store_;
}

GURL DwaReportingService::GetUploadUrl() const {
  return metrics::GetDwaServerUrl();
}

GURL DwaReportingService::GetInsecureUploadUrl() const {
  // Returns an empty string since retrying over HTTP is not enabled for DWA.
  return GURL();
}

std::string_view DwaReportingService::upload_mime_type() const {
  return kMetricsMimeType;
}

metrics::MetricsLogUploader::MetricServiceType
DwaReportingService::service_type() const {
  return MetricsLogUploader::DWA;
}

}  // namespace metrics::dwa
