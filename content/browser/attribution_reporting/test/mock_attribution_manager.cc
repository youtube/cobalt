// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/test/mock_attribution_manager.h"

#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "content/browser/attribution_reporting/attribution_data_host_manager.h"
#include "content/browser/attribution_reporting/attribution_observer.h"
#include "content/browser/attribution_reporting/attribution_reporting.mojom-forward.h"
#include "content/browser/attribution_reporting/os_registration.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

MockAttributionManager::MockAttributionManager() = default;

MockAttributionManager::~MockAttributionManager() = default;

void MockAttributionManager::AddObserver(AttributionObserver* observer) {
  observers_.AddObserver(observer);
}

void MockAttributionManager::RemoveObserver(AttributionObserver* observer) {
  observers_.RemoveObserver(observer);
}

AttributionDataHostManager* MockAttributionManager::GetDataHostManager() {
  DCHECK(data_host_manager_);
  return data_host_manager_.get();
}

void MockAttributionManager::NotifySourcesChanged() {
  for (auto& observer : observers_) {
    observer.OnSourcesChanged();
  }
}

void MockAttributionManager::NotifyReportsChanged() {
  for (auto& observer : observers_) {
    observer.OnReportsChanged();
  }
}

void MockAttributionManager::NotifySourceHandled(
    const StorableSource& source,
    StorableSource::Result result,
    absl::optional<uint64_t> cleared_debug_key) {
  base::Time now = base::Time::Now();
  for (auto& observer : observers_) {
    observer.OnSourceHandled(source, now, cleared_debug_key, result);
  }
}

void MockAttributionManager::NotifyReportSent(const AttributionReport& report,
                                              bool is_debug_report,
                                              const SendResult& info) {
  for (auto& observer : observers_) {
    observer.OnReportSent(report, is_debug_report, info);
  }
}

void MockAttributionManager::NotifyTriggerHandled(
    const AttributionTrigger& trigger,
    const CreateReportResult& result,
    absl::optional<uint64_t> cleared_debug_key) {
  for (auto& observer : observers_) {
    observer.OnTriggerHandled(trigger, cleared_debug_key, result);
  }
}

void MockAttributionManager::NotifyDebugReportSent(
    const AttributionDebugReport& report,
    const int status,
    const base::Time time) {
  for (auto& observer : observers_) {
    observer.OnDebugReportSent(report, status, time);
  }
}

void MockAttributionManager::NotifyOsRegistration(
    const OsRegistration& registration,
    bool is_debug_key_allowed,
    attribution_reporting::mojom::OsRegistrationResult result) {
  base::Time now = base::Time::Now();
  for (auto& observer : observers_) {
    observer.OnOsRegistration(now, registration, is_debug_key_allowed, result);
  }
}

void MockAttributionManager::SetDataHostManager(
    std::unique_ptr<AttributionDataHostManager> manager) {
  DCHECK(manager);
  data_host_manager_ = std::move(manager);
}

}  // namespace content
