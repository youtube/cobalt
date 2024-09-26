// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_MODEL_TYPE_PROCESSOR_H_
#define COMPONENTS_SYNC_ENGINE_MODEL_TYPE_PROCESSOR_H_

#include <memory>
#include <vector>

#include "base/functional/callback_forward.h"
#include "components/sync/engine/commit_and_get_updates_types.h"
#include "components/sync/protocol/data_type_progress_marker.pb.h"
#include "components/sync/protocol/model_type_state.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace syncer {
class CommitQueue;

// Interface used by sync backend to issue requests to a synced data type.
class ModelTypeProcessor {
 public:
  ModelTypeProcessor() = default;
  virtual ~ModelTypeProcessor() = default;

  // Connect this processor to the sync engine via |commit_queue|. Once called,
  // the processor will send any pending and future commits via this channel.
  // This can only be called multiple times if the processor is disconnected
  // (via the DataTypeController) in between.
  virtual void ConnectSync(std::unique_ptr<CommitQueue> commit_queue) = 0;

  // Disconnect this processor from the sync engine. Change metadata will
  // continue being processed and persisted, but no commits can be made until
  // the next time sync is connected.
  virtual void DisconnectSync() = 0;

  // Sync engine calls GetLocalChanges to request local entities to be committed
  // to server. Processor should call callback passing local entites when they
  // are ready. Processor should not pass more than |max_entities|.
  using GetLocalChangesCallback =
      base::OnceCallback<void(CommitRequestDataList&&)>;
  virtual void GetLocalChanges(size_t max_entries,
                               GetLocalChangesCallback callback) = 0;

  // Informs this object that some of its commit requests have been
  // serviced (successfully or not).
  virtual void OnCommitCompleted(
      const sync_pb::ModelTypeState& type_state,
      const CommitResponseDataList& committed_response_list,
      const FailedCommitResponseDataList& error_response_list) = 0;

  // Informs this object that a commit attempt failed, e.g. due to network or
  // server issues. The commit may not include all pending entities.
  virtual void OnCommitFailed(SyncCommitError commit_error) {}

  // Informs this object that there are some incoming updates it should
  // handle.
  virtual void OnUpdateReceived(
      const sync_pb::ModelTypeState& type_state,
      UpdateResponseDataList updates,
      absl::optional<sync_pb::GarbageCollectionDirective> gc_directive) = 0;

  // Informs this object that it should handle new invalidations to store,
  // replacing any previously-stored invalidations.
  virtual void StorePendingInvalidations(
      std::vector<sync_pb::ModelTypeState::Invalidation>
          invalidations_to_store) = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_MODEL_TYPE_PROCESSOR_H_
