// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/structured/oobe_structured_metrics_watcher.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "components/metrics/structured/structured_metrics_service.h"

namespace metrics::structured {

OobeStructuredMetricsWatcher::OobeStructuredMetricsWatcher(
    StructuredMetricsService* service,
    int max_events)
    : max_events_(max_events), service_(service) {}

bool OobeStructuredMetricsWatcher::ShouldProcessOnEventRecord(
    const Event& event) {
  return false;
}

void OobeStructuredMetricsWatcher::OnEventsRecord(Event* event) {}

void OobeStructuredMetricsWatcher::OnEventRecorded(
    StructuredEventProto* event) {
  if (!IsOobeActive()) {
    return;
  }

  event_count_ += 1;

  AttemptUpload();
}

void OobeStructuredMetricsWatcher::OnProvideIndependentMetrics(
    ChromeUserMetricsExtension* uma_proto) {}

bool OobeStructuredMetricsWatcher::IsOobeActive() const {
  return !ash::StartupUtils::IsOobeCompleted();
}

void OobeStructuredMetricsWatcher::OnProfileAdded(
    const base::FilePath& /*path*/) {
  AttemptUpload();
}

bool OobeStructuredMetricsWatcher::ShouldUpload() const {
  return service_->recorder()->can_provide_metrics() &&
         event_count_ >= max_events_;
}

void OobeStructuredMetricsWatcher::AttemptUpload() {
  if (!ShouldUpload()) {
    return;
  }

  service_->ManualUpload();
  event_count_ = 0;
}

}  // namespace metrics::structured
