// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MULTISTEP_FILTER_CORE_PREFS_RETENTION_STATE_SNAPSHOT_H_
#define COMPONENTS_MULTISTEP_FILTER_CORE_PREFS_RETENTION_STATE_SNAPSHOT_H_

namespace multistep_filter {

// Snapshot capturing the persisted Multistep Filter retention history for a
// Profile.
struct RetentionStateSnapshot {
  int suggestion_impressions = 0;
  int suggestion_acceptances = 0;
  bool is_last_suggestion_accepted = false;

  friend bool operator==(const RetentionStateSnapshot&,
                         const RetentionStateSnapshot&) = default;
};

}  // namespace multistep_filter

#endif  // COMPONENTS_MULTISTEP_FILTER_CORE_PREFS_RETENTION_STATE_SNAPSHOT_H_
