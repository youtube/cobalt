// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_logs_event_manager.h"

namespace metrics {

// static
bool MetricsLogsEventManager::ScopedNotifyLogType::instance_exists_ = false;

MetricsLogsEventManager::ScopedNotifyLogType::ScopedNotifyLogType(
    MetricsLogsEventManager* logs_event_manager,
    MetricsLog::LogType log_type)
    : logs_event_manager_(logs_event_manager) {
  DCHECK(!instance_exists_);
  instance_exists_ = true;
  if (logs_event_manager_)
    logs_event_manager_->NotifyLogType(log_type);
}

MetricsLogsEventManager::ScopedNotifyLogType::~ScopedNotifyLogType() {
  DCHECK(instance_exists_);
  if (logs_event_manager_)
    logs_event_manager_->NotifyLogType(absl::nullopt);
  instance_exists_ = false;
}

MetricsLogsEventManager::MetricsLogsEventManager() = default;
MetricsLogsEventManager::~MetricsLogsEventManager() = default;

void MetricsLogsEventManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void MetricsLogsEventManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void MetricsLogsEventManager::NotifyLogCreated(base::StringPiece log_hash,
                                               base::StringPiece log_data,
                                               base::StringPiece log_timestamp,
                                               CreateReason reason) {
  for (Observer& observer : observers_)
    observer.OnLogCreated(log_hash, log_data, log_timestamp, reason);
}

void MetricsLogsEventManager::NotifyLogEvent(LogEvent event,
                                             base::StringPiece log_hash,
                                             base::StringPiece message) {
  for (Observer& observer : observers_)
    observer.OnLogEvent(event, log_hash, message);
}

void MetricsLogsEventManager::NotifyLogType(
    absl::optional<MetricsLog::LogType> log_type) {
  for (Observer& observer : observers_)
    observer.OnLogType(log_type);
}

}  // namespace metrics
