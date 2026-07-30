// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/critical_actions/core/browser/critical_action_backend.h"

#include <utility>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/time/time.h"
#include "components/critical_actions/core/browser/critical_action_database.h"

namespace critical_actions {

CriticalActionBackend::CriticalActionBackend(const base::FilePath& db_path)
    : db_path_(db_path) {
  // Detach from the construction sequence since the backend is designed to run
  // on a separate background task runner sequence.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

CriticalActionBackend::~CriticalActionBackend() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool CriticalActionBackend::Init() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!db_);
  auto db = std::make_unique<CriticalActionDatabase>(db_path_);
  if (!db->Init()) {
    return false;
  }
  db_ = std::move(db);
  return true;
}

bool CriticalActionBackend::AddCriticalAction(
    const CriticalActionEntry& entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_) {
    return false;
  }
  return db_->AddCriticalAction(entry);
}

std::optional<CriticalActionEntry> CriticalActionBackend::GetCriticalAction(
    std::string_view critical_action_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_) {
    return std::nullopt;
  }
  return db_->GetCriticalAction(critical_action_id);
}

bool CriticalActionBackend::DeleteCriticalAction(
    std::string_view critical_action_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_) {
    return false;
  }
  return db_->DeleteCriticalAction(critical_action_id);
}

bool CriticalActionBackend::DeleteCriticalActionsInTimeRange(
    base::Time start_time,
    base::Time end_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_) {
    return false;
  }
  return db_->DeleteCriticalActionsInTimeRange(start_time, end_time);
}

}  // namespace critical_actions
