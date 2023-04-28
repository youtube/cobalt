// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_METRICS_LOG_STORE_H_
#define COMPONENTS_METRICS_METRICS_LOG_STORE_H_

#include <string>

#include "base/macros.h"
#include "components/metrics/log_store.h"
#include "components/metrics/metrics_log.h"
#include "components/metrics/persisted_logs.h"

class PrefService;
class PrefRegistrySimple;

namespace metrics {

// A LogStore implementation for storing UMA logs.
// This implementation keeps track of two types of logs, initial and ongoing,
// each stored in PersistedLogs.  It prioritizes staging initial logs over
// ongoing logs.
class MetricsLogStore : public LogStore {
 public:
  // Constructs a MetricsLogStore that persists data into |local_state|.
  // If max_log_size is non-zero, it will not persist ongoing logs larger than
  // |max_ongoing_log_size| bytes.
  MetricsLogStore(PrefService* local_state, size_t max_ongoing_log_size);
  ~MetricsLogStore();

  // Registers local state prefs used by this class.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Saves |log_data| as the given type.
  void StoreLog(const std::string& log_data, MetricsLog::LogType log_type);

  // LogStore:
  bool has_unsent_logs() const override;
  bool has_staged_log() const override;
  const std::string& staged_log() const override;
  const std::string& staged_log_hash() const override;
  void StageNextLog() override;
  void DiscardStagedLog() override;
  void PersistUnsentLogs() const override;
  void LoadPersistedUnsentLogs() override;

  // Inspection methods for tests.
  size_t ongoing_log_count() const { return ongoing_log_queue_.size(); }
  size_t initial_log_count() const { return initial_log_queue_.size(); }

 private:
  // Tracks whether unsent logs (if any) have been loaded from the serializer.
  bool unsent_logs_loaded_;

  // Logs stored with the INITIAL_STABILITY_LOG type that haven't been sent yet.
  // These logs will be staged first when staging new logs.
  PersistedLogs initial_log_queue_;
  // Logs stored with the ONGOING_LOG type that haven't been sent yet.
  PersistedLogs ongoing_log_queue_;

  DISALLOW_COPY_AND_ASSIGN(MetricsLogStore);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_METRICS_LOG_STORE_H_
