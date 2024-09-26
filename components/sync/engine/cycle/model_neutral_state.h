// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_CYCLE_MODEL_NEUTRAL_STATE_H_
#define COMPONENTS_SYNC_ENGINE_CYCLE_MODEL_NEUTRAL_STATE_H_

#include "components/sync/base/model_type.h"
#include "components/sync/base/syncer_error.h"

namespace syncer {

// Grouping of all state that applies to all model types.  Note that some
// components of the global grouping can internally implement finer grained
// scope control, but the top level entity is still a singleton with respect to
// model types.
struct ModelNeutralState {
  ModelNeutralState();
  ModelNeutralState(const ModelNeutralState& other);
  ~ModelNeutralState();

  // The set of types for which non-deletion updates were returned from the
  // server.
  ModelTypeSet updated_types;

  int num_successful_commits;

  // This is needed for monitoring extensions activity.
  int num_successful_bookmark_commits;

  // Download event counters.
  int num_updates_downloaded_total;
  int num_tombstone_updates_downloaded_total;

  // Update application and conflicts.
  int num_server_conflicts;

  // Records the most recent results of GetKey, PostCommit and GetUpdates
  // commands.
  SyncerError last_get_key_result;
  SyncerError last_download_updates_result;
  SyncerError commit_result;

  // Set to true by PostCommitMessageCommand if any commits were successful.
  bool items_committed;
};

bool HasSyncerError(const ModelNeutralState& state);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_CYCLE_MODEL_NEUTRAL_STATE_H_
