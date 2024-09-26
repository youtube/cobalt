// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_MODEL_TYPE_PROCESSOR_PROXY_H_
#define COMPONENTS_SYNC_ENGINE_MODEL_TYPE_PROCESSOR_PROXY_H_

#include <memory>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/sync/engine/model_type_processor.h"

namespace syncer {

class ModelTypeProcessorProxy : public ModelTypeProcessor {
 public:
  ModelTypeProcessorProxy(
      const base::WeakPtr<ModelTypeProcessor>& processor,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);
  ~ModelTypeProcessorProxy() override;

  void ConnectSync(std::unique_ptr<CommitQueue> worker) override;
  void DisconnectSync() override;
  void GetLocalChanges(size_t max_entries,
                       GetLocalChangesCallback callback) override;
  void OnCommitCompleted(
      const sync_pb::ModelTypeState& type_state,
      const CommitResponseDataList& committed_response_list,
      const FailedCommitResponseDataList& error_response_list) override;
  void OnCommitFailed(SyncCommitError commit_error) override;
  void OnUpdateReceived(const sync_pb::ModelTypeState& type_state,
                        UpdateResponseDataList updates,
                        absl::optional<sync_pb::GarbageCollectionDirective>
                            gc_directive) override;
  void StorePendingInvalidations(
      std::vector<sync_pb::ModelTypeState::Invalidation> invalidations_to_store)
      override;

 private:
  base::WeakPtr<ModelTypeProcessor> processor_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_MODEL_TYPE_PROCESSOR_PROXY_H_
