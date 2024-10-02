// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/nigori/nigori_model_type_processor.h"

#include <vector>

#include "base/logging.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/data_type_histogram.h"
#include "components/sync/base/time.h"
#include "components/sync/engine/commit_queue.h"
#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/engine/forwarding_model_type_processor.h"
#include "components/sync/engine/model_type_processor_metrics.h"
#include "components/sync/model/processor_entity.h"
#include "components/sync/model/type_entities_count.h"
#include "components/sync/nigori/nigori_sync_bridge.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/model_type_state_helper.h"
#include "components/sync/protocol/proto_memory_estimations.h"
#include "components/sync/protocol/proto_value_conversions.h"

namespace syncer {

namespace {

// TODO(mamir): remove those and adjust the code accordingly. Similarly in
// tests.
const char kNigoriStorageKey[] = "NigoriStorageKey";
const char kRawNigoriClientTagHash[] = "NigoriClientTagHash";

}  // namespace

NigoriModelTypeProcessor::NigoriModelTypeProcessor() : bridge_(nullptr) {}

NigoriModelTypeProcessor::~NigoriModelTypeProcessor() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void NigoriModelTypeProcessor::ConnectSync(
    std::unique_ptr<CommitQueue> worker) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << "Successfully connected Encryption Keys";

  worker_ = std::move(worker);
  NudgeForCommitIfNeeded();
}

void NigoriModelTypeProcessor::DisconnectSync() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsConnected());

  DVLOG(1) << "Disconnecting sync for Encryption Keys";
  worker_.reset();
  if (entity_) {
    entity_->ClearTransientSyncState();
  }
}

void NigoriModelTypeProcessor::GetLocalChanges(
    size_t max_entries,
    GetLocalChangesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GT(max_entries, 0U);
  // If there is a model error, it must have been reported already but hasn't
  // reached the sync engine yet. In this case return directly to avoid
  // interactions with the bridge.
  if (model_error_) {
    std::move(callback).Run(CommitRequestDataList());
    return;
  }

  DCHECK(entity_);

  // No local changes to commit.
  if (!entity_->RequiresCommitRequest()) {
    std::move(callback).Run(CommitRequestDataList());
    return;
  }

  if (entity_->RequiresCommitData()) {
    // SetCommitData will update EntityData's fields with values from
    // metadata.
    entity_->SetCommitData(bridge_->GetData());
  }

  auto commit_request_data = std::make_unique<CommitRequestData>();
  entity_->InitializeCommitRequestData(commit_request_data.get());

  CommitRequestDataList commit_request_data_list;
  commit_request_data_list.push_back(std::move(commit_request_data));

  std::move(callback).Run(std::move(commit_request_data_list));
}

void NigoriModelTypeProcessor::OnCommitCompleted(
    const sync_pb::ModelTypeState& type_state,
    const CommitResponseDataList& committed_response_list,
    const FailedCommitResponseDataList& error_response_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(entity_);

  model_type_state_ = type_state;
  if (!committed_response_list.empty()) {
    entity_->ReceiveCommitResponse(committed_response_list[0],
                                   /*commit_only=*/false);
  } else {
    // If the entity hasn't been mentioned in response_list, then it's not
    // committed and we should reset its commit_requested_sequence_number so
    // they are committed again on next sync cycle.
    entity_->ClearTransientSyncState();
  }
  // Ask the bridge to persist the new metadata.
  bridge_->ApplyIncrementalSyncChanges(/*data=*/absl::nullopt);
}

void NigoriModelTypeProcessor::OnUpdateReceived(
    const sync_pb::ModelTypeState& type_state,
    UpdateResponseDataList updates,
    absl::optional<sync_pb::GarbageCollectionDirective> gc_directive) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(model_ready_to_sync_);
  // If there is a model error, it must have been reported already but hasn't
  // reached the sync engine yet. In this case return directly to avoid
  // interactions with the bridge.
  if (model_error_) {
    return;
  }

  // TODO(crbug.com/1356900): validate incoming updates, e.g. |gc_directive|
  // must be empty for Nigori.
  absl::optional<ModelError> error;

  const bool is_initial_sync =
      !IsInitialSyncDone(model_type_state_.initial_sync_state());
  LogUpdatesReceivedByProcessorHistogram(NIGORI, is_initial_sync,
                                         updates.size());

  model_type_state_ = type_state;

  if (is_initial_sync) {
    DCHECK(!entity_);
    if (updates.empty()) {
      error = bridge_->MergeFullSyncData(absl::nullopt);
    } else {
      DCHECK(!updates[0].entity.is_deleted());
      entity_ = ProcessorEntity::CreateNew(
          kNigoriStorageKey, ClientTagHash::FromHashed(kRawNigoriClientTagHash),
          updates[0].entity.id, updates[0].entity.creation_time);
      entity_->RecordAcceptedRemoteUpdate(updates[0], /*trimmed_specifics=*/{});
      error = bridge_->MergeFullSyncData(std::move(updates[0].entity));
    }
    if (error) {
      ReportError(*error);
    }
    return;
  }

  if (updates.empty()) {
    bridge_->ApplyIncrementalSyncChanges(/*data=*/absl::nullopt);
    return;
  }

  DCHECK(entity_);
  // We assume the bridge will issue errors in case of deletions. Therefore, we
  // are adding the following DCHECK to simplify the code.
  DCHECK(!updates[0].entity.is_deleted());

  if (entity_->IsVersionAlreadyKnown(updates[0].response_version)) {
    // Seen this update before; just ignore it.
    bridge_->ApplyIncrementalSyncChanges(/*data=*/absl::nullopt);
    return;
  }

  if (entity_->IsUnsynced()) {
    // Remote update always win in case of conflict, because bridge takes care
    // of reapplying pending local changes after processing the remote update.
    entity_->RecordForcedRemoteUpdate(updates[0], /*trimmed_specifics=*/{});
    error = bridge_->ApplyIncrementalSyncChanges(std::move(updates[0].entity));
  } else if (!entity_->MatchesData(updates[0].entity)) {
    // Inform the bridge of the new or updated data.
    entity_->RecordAcceptedRemoteUpdate(updates[0], /*trimmed_specifics=*/{});
    error = bridge_->ApplyIncrementalSyncChanges(std::move(updates[0].entity));
  }

  if (error) {
    ReportError(*error);
    return;
  }

  // There may be new reasons to commit by the time this function is done.
  NudgeForCommitIfNeeded();
}

void NigoriModelTypeProcessor::StorePendingInvalidations(
    std::vector<sync_pb::ModelTypeState::Invalidation> invalidations_to_store) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  model_type_state_.mutable_invalidations()->Assign(
      invalidations_to_store.begin(), invalidations_to_store.end());
  // ApplyIncrementalSyncChanges does actually query and persist the
  // |model_type_state_|.
  bridge_->ApplyIncrementalSyncChanges(/*data=*/absl::nullopt);
}

void NigoriModelTypeProcessor::OnSyncStarting(
    const DataTypeActivationRequest& request,
    StartCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << "Sync is starting for Encryption Keys";
  DCHECK(request.error_handler) << "Encryption Keys";
  DCHECK(callback) << "Encryption Keys";
  DCHECK(!start_callback_) << "Encryption Keys";
  DCHECK(!IsConnected()) << "Encryption Keys";

  start_callback_ = std::move(callback);
  activation_request_ = request;

  ConnectIfReady();
}

void NigoriModelTypeProcessor::OnSyncStopping(
    SyncStopMetadataFate metadata_fate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Disabling sync for a type shouldn't happen before the model is loaded
  // because OnSyncStopping() is not allowed to be called before
  // OnSyncStarting() has completed.
  DCHECK(!start_callback_);

  worker_.reset();

  switch (metadata_fate) {
    case syncer::KEEP_METADATA: {
      break;
    }

    case syncer::CLEAR_METADATA: {
      ClearMetadataAndReset();
      model_ready_to_sync_ = false;

      // The model is still ready to sync (with the same |bridge_|) and same
      // sync metadata.
      ModelReadyToSync(bridge_, NigoriMetadataBatch());
      break;
    }
  }
}

void NigoriModelTypeProcessor::GetAllNodesForDebugging(
    AllNodesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::unique_ptr<EntityData> entity_data = bridge_->GetData();
  if (!entity_data) {
    std::move(callback).Run(syncer::NIGORI, base::Value::List());
    return;
  }

  if (entity_) {
    const sync_pb::EntityMetadata& metadata = entity_->metadata();
    // Set id value as the legacy Directory implementation, "s" means server.
    entity_data->id = "s" + metadata.server_id();
    entity_data->creation_time = ProtoTimeToTime(metadata.creation_time());
    entity_data->modification_time =
        ProtoTimeToTime(metadata.modification_time());
  }
  base::Value::Dict root_node = entity_data->ToDictionaryValue();
  if (entity_) {
    root_node.Set("metadata", EntityMetadataToValue(entity_->metadata()));
  }

  // Function isTypeRootNode in sync_node_browser.js use PARENT_ID and
  // UNIQUE_SERVER_TAG to check if the node is root node. isChildOf in
  // sync_node_browser.js uses modelType to check if root node is parent of real
  // data node.
  root_node.Set("PARENT_ID", "r");
  root_node.Set("UNIQUE_SERVER_TAG", "Nigori");
  root_node.Set("modelType", ModelTypeToDebugString(NIGORI));

  base::Value::List all_nodes;
  all_nodes.Append(std::move(root_node));
  std::move(callback).Run(syncer::NIGORI, std::move(all_nodes));
}

void NigoriModelTypeProcessor::GetTypeEntitiesCountForDebugging(
    base::OnceCallback<void(const TypeEntitiesCount&)> callback) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TypeEntitiesCount count(syncer::NIGORI);
  count.non_tombstone_entities = entity_ ? 1 : 0;
  count.entities = count.non_tombstone_entities;
  std::move(callback).Run(count);
}

void NigoriModelTypeProcessor::RecordMemoryUsageAndCountsHistograms() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  size_t memory_usage = 0;
  memory_usage += EstimateMemoryUsage(model_type_state_);
  memory_usage += entity_ ? entity_->EstimateMemoryUsage() : 0;
  SyncRecordModelTypeMemoryHistogram(ModelType::NIGORI, memory_usage);
  SyncRecordModelTypeCountHistogram(ModelType::NIGORI, entity_ ? 1 : 0);
}

void NigoriModelTypeProcessor::ModelReadyToSync(
    NigoriSyncBridge* bridge,
    NigoriMetadataBatch nigori_metadata) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(bridge);
  DCHECK(!model_ready_to_sync_);
  bridge_ = bridge;
  model_ready_to_sync_ = true;

  // Abort if the model already experienced an error.
  if (model_error_) {
    return;
  }

  MigrateLegacyInitialSyncDone(nigori_metadata.model_type_state, NIGORI);

  if (IsInitialSyncDone(
          nigori_metadata.model_type_state.initial_sync_state()) &&
      nigori_metadata.entity_metadata) {
    model_type_state_ = std::move(nigori_metadata.model_type_state);
    sync_pb::EntityMetadata metadata =
        std::move(*nigori_metadata.entity_metadata);
    metadata.set_client_tag_hash(kRawNigoriClientTagHash);
    entity_ = ProcessorEntity::CreateFromMetadata(kNigoriStorageKey,
                                                  std::move(metadata));
  } else {
    // First time syncing or persisted data are corrupted; initialize metadata.
    model_type_state_.mutable_progress_marker()->set_data_type_id(
        sync_pb::EntitySpecifics::kNigoriFieldNumber);
  }
  ConnectIfReady();
}

void NigoriModelTypeProcessor::Put(std::unique_ptr<EntityData> entity_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(entity_data);
  DCHECK(!entity_data->is_deleted());
  DCHECK(!entity_data->name.empty());
  DCHECK(!entity_data->specifics.has_encrypted());
  DCHECK_EQ(NIGORI, GetModelTypeFromSpecifics(entity_data->specifics));
  DCHECK(entity_);

  if (!IsInitialSyncDone(model_type_state_.initial_sync_state())) {
    // Ignore changes before the initial sync is done.
    return;
  }

  if (entity_->MatchesData(*entity_data)) {
    // Ignore changes that don't actually change anything.
    return;
  }

  entity_->RecordLocalUpdate(std::move(entity_data), /*trimmed_specifics=*/{});
  NudgeForCommitIfNeeded();
}

bool NigoriModelTypeProcessor::IsEntityUnsynced() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (entity_ == nullptr) {
    return false;
  }

  return entity_->IsUnsynced();
}

NigoriMetadataBatch NigoriModelTypeProcessor::GetMetadata() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsTrackingMetadata());
  DCHECK(entity_);

  NigoriMetadataBatch nigori_metadata_batch;
  nigori_metadata_batch.model_type_state = model_type_state_;
  nigori_metadata_batch.entity_metadata = entity_->metadata();

  return nigori_metadata_batch;
}

void NigoriModelTypeProcessor::ReportError(const ModelError& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Ignore all errors after the first.
  if (model_error_) {
    return;
  }

  model_error_ = error;

  if (IsConnected()) {
    DisconnectSync();
  }
  // Shouldn't connect anymore.
  start_callback_.Reset();
  if (activation_request_.error_handler) {
    // Tell sync about the error.
    activation_request_.error_handler.Run(error);
  }
}

base::WeakPtr<ModelTypeControllerDelegate>
NigoriModelTypeProcessor::GetControllerDelegate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_ptr_factory_for_controller_.GetWeakPtr();
}

bool NigoriModelTypeProcessor::IsConnectedForTest() const {
  return IsConnected();
}

const sync_pb::ModelTypeState&
NigoriModelTypeProcessor::GetModelTypeStateForTest() {
  return model_type_state_;
}

bool NigoriModelTypeProcessor::IsTrackingMetadata() {
  return IsInitialSyncDone(model_type_state_.initial_sync_state());
}

bool NigoriModelTypeProcessor::IsConnected() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return worker_ != nullptr;
}

void NigoriModelTypeProcessor::ConnectIfReady() {
  if (!start_callback_) {
    return;
  }
  if (model_error_) {
    activation_request_.error_handler.Run(model_error_.value());
    start_callback_.Reset();
    return;
  }
  if (!model_ready_to_sync_) {
    return;
  }

  if (IsInitialSyncDone(model_type_state_.initial_sync_state()) &&
      model_type_state_.cache_guid() != activation_request_.cache_guid) {
    ClearMetadataAndReset();
    DCHECK(model_ready_to_sync_);
  }
  model_type_state_.set_cache_guid(activation_request_.cache_guid);

  // Cache GUID verification earlier above guarantees the user is the same.
  model_type_state_.set_authenticated_account_id(
      activation_request_.authenticated_account_id.ToString());

  auto activation_response = std::make_unique<DataTypeActivationResponse>();
  activation_response->model_type_state = model_type_state_;
  activation_response->type_processor =
      std::make_unique<ForwardingModelTypeProcessor>(this);
  std::move(start_callback_).Run(std::move(activation_response));
}

void NigoriModelTypeProcessor::NudgeForCommitIfNeeded() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Don't bother sending anything if there's no one to send to.
  if (!IsConnected()) {
    return;
  }

  // Don't send anything if the type is not ready to handle commits.
  if (!IsInitialSyncDone(model_type_state_.initial_sync_state())) {
    return;
  }

  // Nudge worker if there are any entities with local changes.
  if (entity_->RequiresCommitRequest()) {
    worker_->NudgeForCommit();
  }
}

void NigoriModelTypeProcessor::ClearMetadataAndReset() {
  // The bridge is responsible for deleting all data and metadata upon
  // disabling sync.
  bridge_->ApplyDisableSyncChanges();
  entity_.reset();
  model_type_state_ = sync_pb::ModelTypeState();
  model_type_state_.mutable_progress_marker()->set_data_type_id(
      sync_pb::EntitySpecifics::kNigoriFieldNumber);
}

void NigoriModelTypeProcessor::ClearMetadataWhileStopped() {
  NOTREACHED() << "Nigori has a separate load callback and way to clear data. "
                  "This method should not have been called.";
}

}  // namespace syncer
