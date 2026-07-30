// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRITICAL_ACTIONS_CORE_BROWSER_CRITICAL_ACTION_BACKEND_H_
#define COMPONENTS_CRITICAL_ACTIONS_CORE_BROWSER_CRITICAL_ACTION_BACKEND_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/files/file_path.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "components/critical_actions/core/browser/critical_action_types.h"

namespace critical_actions {

class CriticalActionDatabase;

// Internal implementation of Critical Action History which does the work on the
// database. This runs on a background sequenced task runner (to avoid blocking
// the main thread during database operations) and is NOT thread-safe, so it
// must only be called on that background sequence.
//
// Most functions here correspond to the public asynchronous APIs exposed by the
// service layer.
class CriticalActionBackend {
 public:
  explicit CriticalActionBackend(const base::FilePath& db_path);
  CriticalActionBackend(const CriticalActionBackend&) = delete;
  CriticalActionBackend& operator=(const CriticalActionBackend&) = delete;
  ~CriticalActionBackend();

  // Initializes the underlying critical action database. Performs required
  // SQLite table creations/schema setup.
  bool Init();

  // Inserts a new critical action record.
  bool AddCriticalAction(const CriticalActionEntry& entry);

  // Retrieves a critical action record by its client UUID.
  std::optional<CriticalActionEntry> GetCriticalAction(
      std::string_view critical_action_id);

  // Deletes a single critical action record.
  bool DeleteCriticalAction(std::string_view critical_action_id);

  // Deletes all critical action records within the given time range,
  // inclusive of start_time and exclusive of end_time.
  bool DeleteCriticalActionsInTimeRange(base::Time start_time,
                                        base::Time end_time);

 private:
  const base::FilePath db_path_;
  std::unique_ptr<CriticalActionDatabase> db_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace critical_actions

#endif  // COMPONENTS_CRITICAL_ACTIONS_CORE_BROWSER_CRITICAL_ACTION_BACKEND_H_
