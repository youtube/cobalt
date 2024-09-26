// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/glue/sync_engine_backend.h"

#include <utility>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/sequenced_task_runner.h"
#include "components/invalidation/impl/invalidation_switches.h"
#include "components/invalidation/public/invalidation_util.h"
#include "components/sync/base/features.h"
#include "components/sync/base/invalidation_adapter.h"
#include "components/sync/base/legacy_directory_deletion.h"
#include "components/sync/base/sync_invalidation_adapter.h"
#include "components/sync/base/sync_stop_metadata_fate.h"
#include "components/sync/driver/configure_context.h"
#include "components/sync/driver/glue/sync_engine_impl.h"
#include "components/sync/driver/model_type_controller.h"
#include "components/sync/engine/cycle/sync_cycle_snapshot.h"
#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/engine/engine_components_factory.h"
#include "components/sync/engine/events/protocol_event.h"
#include "components/sync/engine/net/http_post_provider_factory.h"
#include "components/sync/engine/shutdown_reason.h"
#include "components/sync/engine/sync_manager.h"
#include "components/sync/engine/sync_manager_factory.h"
#include "components/sync/model/forwarding_model_type_controller_delegate.h"
#include "components/sync/nigori/nigori_model_type_processor.h"
#include "components/sync/nigori/nigori_storage_impl.h"
#include "components/sync/nigori/nigori_sync_bridge_impl.h"
#include "components/sync/protocol/sync_invalidations_payload.pb.h"

// Helper macros to log with the syncer thread name; useful when there
// are multiple syncers involved.

#define SDVLOG(verbose_level) DVLOG(verbose_level) << name_ << ": "

namespace syncer {

namespace {

const base::FilePath::CharType kNigoriStorageFilename[] =
    FILE_PATH_LITERAL("Nigori.bin");

void RecordInvalidationPerModelType(ModelType type) {
  UMA_HISTOGRAM_ENUMERATION("Sync.InvalidationPerModelType",
                            ModelTypeHistogramValue(type));
}

void RecordIncomingInvalidationStatus(
    SyncEngineBackend::IncomingInvalidationStatus status) {
  UMA_HISTOGRAM_ENUMERATION("Sync.IncomingInvalidationStatus", status);
}

}  // namespace

SyncEngineBackend::RestoredLocalTransportData::RestoredLocalTransportData() =
    default;

SyncEngineBackend::RestoredLocalTransportData::RestoredLocalTransportData(
    RestoredLocalTransportData&&) = default;

SyncEngineBackend::RestoredLocalTransportData::~RestoredLocalTransportData() =
    default;

SyncEngineBackend::SyncEngineBackend(const std::string& name,
                                     const base::FilePath& sync_data_folder,
                                     const base::WeakPtr<SyncEngineImpl>& host)
    : name_(name), sync_data_folder_(sync_data_folder), host_(host) {
  DCHECK(host);
  // This is constructed on the UI thread but used from another thread.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

SyncEngineBackend::~SyncEngineBackend() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void SyncEngineBackend::OnSyncCycleCompleted(
    const SyncCycleSnapshot& snapshot) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  host_.Call(FROM_HERE, &SyncEngineImpl::HandleSyncCycleCompletedOnFrontendLoop,
             snapshot);
}

void SyncEngineBackend::DoRefreshTypes(ModelTypeSet types) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sync_manager_->RefreshTypes(types);
}

void SyncEngineBackend::OnConnectionStatusChange(ConnectionStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  host_.Call(FROM_HERE,
             &SyncEngineImpl::HandleConnectionStatusChangeOnFrontendLoop,
             status);
}

void SyncEngineBackend::OnActionableProtocolError(
    const SyncProtocolError& sync_error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  host_.Call(FROM_HERE,
             &SyncEngineImpl::HandleActionableProtocolErrorEventOnFrontendLoop,
             sync_error);
}

void SyncEngineBackend::OnMigrationRequested(ModelTypeSet types) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  host_.Call(FROM_HERE, &SyncEngineImpl::HandleMigrationRequestedOnFrontendLoop,
             types);
}

void SyncEngineBackend::OnProtocolEvent(const ProtocolEvent& event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (forward_protocol_events_) {
    std::unique_ptr<ProtocolEvent> event_clone(event.Clone());
    host_.Call(FROM_HERE, &SyncEngineImpl::HandleProtocolEventOnFrontendLoop,
               std::move(event_clone));
  }
}

void SyncEngineBackend::OnSyncStatusChanged(const SyncStatus& status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  host_.Call(FROM_HERE, &SyncEngineImpl::HandleSyncStatusChanged, status);
}

void SyncEngineBackend::DoOnInvalidatorStateChange(
    invalidation::InvalidatorState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sync_manager_->SetInvalidatorEnabled(state ==
                                       invalidation::INVALIDATIONS_ENABLED);
}

void SyncEngineBackend::DoOnIncomingInvalidation(
    const invalidation::TopicInvalidationMap& invalidation_map) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (const invalidation::Topic& topic : invalidation_map.GetTopics()) {
    ModelType type;
    if (!NotificationTypeToRealModelType(topic, &type)) {
      DLOG(WARNING) << "Notification has invalid topic: " << topic;
      RecordIncomingInvalidationStatus(
          IncomingInvalidationStatus::kUnknownModelType);
    } else {
      RecordInvalidationPerModelType(type);
      invalidation::SingleTopicInvalidationSet invalidation_set =
          invalidation_map.ForTopic(topic);
      for (invalidation::Invalidation invalidation : invalidation_set) {
        // Topic-based invalidations contain only one data type, hence this
        // metric should be recorded for each incoming invalidation to represent
        // all incoming messages.
        RecordIncomingInvalidationStatus(IncomingInvalidationStatus::kSuccess);

        std::unique_ptr<SyncInvalidation> inv_adapter(
            new InvalidationAdapter(invalidation));
        sync_manager_->OnIncomingInvalidation(type, std::move(inv_adapter));
      }
    }
  }
}

void SyncEngineBackend::DoInitialize(
    SyncEngine::InitParams params,
    RestoredLocalTransportData restored_local_transport_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Make sure that the directory exists before initializing the backend.
  // If it already exists, this will do no harm.
  if (!base::CreateDirectory(sync_data_folder_)) {
    DLOG(FATAL) << "Sync Data directory creation failed.";
  }

  authenticated_account_id_ = params.authenticated_account_info.account_id;

  auto nigori_processor = std::make_unique<NigoriModelTypeProcessor>();
  nigori_controller_ = std::make_unique<ModelTypeController>(
      NIGORI, std::make_unique<ForwardingModelTypeControllerDelegate>(
                  nigori_processor->GetControllerDelegate().get()));
  sync_encryption_handler_ = std::make_unique<NigoriSyncBridgeImpl>(
      std::move(nigori_processor),
      std::make_unique<NigoriStorageImpl>(
          sync_data_folder_.Append(kNigoriStorageFilename)));

  sync_manager_ = params.sync_manager_factory->CreateSyncManager(name_);
  sync_manager_->AddObserver(this);

  SyncManager::InitArgs args;
  args.service_url = params.service_url;
  args.enable_local_sync_backend = params.enable_local_sync_backend;
  args.local_sync_backend_folder = params.local_sync_backend_folder;
  args.post_factory = std::move(params.http_factory_getter).Run();
  args.encryption_observer_proxy = std::move(params.encryption_observer_proxy);
  args.extensions_activity = params.extensions_activity.get();
  args.invalidator_client_id = params.invalidator_client_id;
  args.engine_components_factory = std::move(params.engine_components_factory);
  args.encryption_handler = sync_encryption_handler_.get();
  args.cancelation_signal = &stop_syncing_signal_;
  args.poll_interval = restored_local_transport_data.poll_interval;
  args.cache_guid = restored_local_transport_data.cache_guid;
  args.birthday = restored_local_transport_data.birthday;
  args.bag_of_chips = restored_local_transport_data.bag_of_chips;
  sync_manager_->Init(&args);

  LoadAndConnectNigoriController();

  ConfigureReason reason = sync_manager_->InitialSyncEndedTypes().Empty()
                               ? CONFIGURE_REASON_NEW_CLIENT
                               : CONFIGURE_REASON_NEWLY_ENABLED_DATA_TYPE;

  ModelTypeSet new_control_types =
      Difference(ControlTypes(), sync_manager_->InitialSyncEndedTypes());

  SDVLOG(1) << "Control Types " << ModelTypeSetToDebugString(new_control_types)
            << " added; calling ConfigureSyncer";

  sync_manager_->ConfigureSyncer(
      reason, new_control_types, SyncManager::SyncFeatureState::INITIALIZING,
      base::BindOnce(&SyncEngineBackend::DoInitialProcessControlTypes,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SyncEngineBackend::DoUpdateCredentials(
    const SyncCredentials& credentials) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // UpdateCredentials can be called during backend initialization, possibly
  // when backend initialization has failed but hasn't notified the UI thread
  // yet. In that case, the sync manager may have been destroyed on another
  // thread before this task was executed, so we do nothing.
  if (sync_manager_) {
    sync_manager_->UpdateCredentials(credentials);
  }
}

void SyncEngineBackend::DoInvalidateCredentials() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (sync_manager_) {
    sync_manager_->InvalidateCredentials();
  }
}

void SyncEngineBackend::DoStartConfiguration() {
  sync_manager_->StartConfiguration();
}

void SyncEngineBackend::DoStartSyncing(base::Time last_poll_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sync_manager_->StartSyncingNormally(last_poll_time);
}

void SyncEngineBackend::DoSetEncryptionPassphrase(
    const std::string& passphrase,
    const KeyDerivationParams& key_derivation_params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sync_manager_->GetEncryptionHandler()->SetEncryptionPassphrase(
      passphrase, key_derivation_params);
}

void SyncEngineBackend::DoAddTrustedVaultDecryptionKeys(
    const std::vector<std::vector<uint8_t>>& keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sync_manager_->GetEncryptionHandler()->AddTrustedVaultDecryptionKeys(keys);
}

void SyncEngineBackend::DoInitialProcessControlTypes() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DVLOG(1) << "Initilalizing Control Types";

  // Initialize encryption.
  sync_manager_->GetEncryptionHandler()->NotifyInitialStateToObservers();

  if (!sync_manager_->InitialSyncEndedTypes().HasAll(ControlTypes())) {
    LOG(ERROR) << "Failed to download control types";
    host_.Call(FROM_HERE,
               &SyncEngineImpl::HandleInitializationFailureOnFrontendLoop);
    return;
  }

  host_.Call(FROM_HERE,
             &SyncEngineImpl::HandleInitializationSuccessOnFrontendLoop,
             sync_manager_->GetModelTypeConnectorProxy(),
             sync_manager_->birthday(), sync_manager_->bag_of_chips());
}

void SyncEngineBackend::DoSetExplicitPassphraseDecryptionKey(
    std::unique_ptr<Nigori> key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sync_manager_->GetEncryptionHandler()->SetExplicitPassphraseDecryptionKey(
      std::move(key));
}

void SyncEngineBackend::ShutdownOnUIThread() {
  // This will cut short any blocking network tasks, cut short any in-progress
  // sync cycles, and prevent the creation of new blocking network tasks and new
  // sync cycles.  If there was an in-progress network request, it would have
  // had a reference to the RequestContextGetter.  This reference will be
  // dropped by the time this function returns.
  //
  // It is safe to call this even if Sync's backend classes have not been
  // initialized yet.  Those classes will receive the message when the sync
  // thread finally getes around to constructing them.
  stop_syncing_signal_.Signal();
}

void SyncEngineBackend::DoShutdown(ShutdownReason reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // |sync_manager_| and |nigori_controller_| may be null if DoInitialize() was
  // never called.
  if (sync_manager_) {
    // However, |sync_manager_| and |nigori_controller_| are always either both
    // null or both non-null.
    DCHECK(nigori_controller_);

    sync_manager_->GetModelTypeConnector()->DisconnectDataType(NIGORI);

    if (reason != ShutdownReason::BROWSER_SHUTDOWN_AND_KEEP_DATA) {
      nigori_controller_->Stop(ShutdownReasonToSyncStopMetadataFate(reason),
                               base::DoNothing());
    }

    nigori_controller_.reset();

    sync_manager_->RemoveObserver(this);
    sync_manager_->ShutdownOnSyncThread();
    sync_manager_.reset();
  }

  if (reason == ShutdownReason::DISABLE_SYNC_AND_CLEAR_DATA) {
    DeleteLegacyDirectoryFilesAndNigoriStorage(sync_data_folder_);
  }

  host_.Reset();
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void SyncEngineBackend::DoPurgeDisabledTypes(const ModelTypeSet& to_purge) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (to_purge.Has(NIGORI)) {
    // We are using USS implementation of Nigori and someone asked us to purge
    // it's data. For regular datatypes it's controlled DataTypeManager, but
    // for Nigori we need to do it here.
    // TODO(crbug.com/1142771): try to find better way to implement this logic,
    // it's likely happen only due to BackendMigrator.
    // TODO(crbug.com/1142771): Evaluate whether this logic is necessary at all.
    // There's no "purging" logic for any other data type, so likely it's not
    // necessary for NIGORI either.
    sync_manager_->GetModelTypeConnector()->DisconnectDataType(NIGORI);
    nigori_controller_->Stop(SyncStopMetadataFate::CLEAR_METADATA,
                             base::DoNothing());
    LoadAndConnectNigoriController();
  }
}

void SyncEngineBackend::DoConfigureSyncer(
    ModelTypeConfigurer::ConfigureParams params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!params.ready_task.is_null());

  base::OnceClosure chained_ready_task(
      base::BindOnce(&SyncEngineBackend::DoFinishConfigureDataTypes,
                     weak_ptr_factory_.GetWeakPtr(), params.to_download,
                     std::move(params.ready_task)));

  sync_manager_->ConfigureSyncer(params.reason, params.to_download,
                                 params.is_sync_feature_enabled
                                     ? SyncManager::SyncFeatureState::ON
                                     : SyncManager::SyncFeatureState::OFF,
                                 std::move(chained_ready_task));
}

void SyncEngineBackend::DoFinishConfigureDataTypes(
    ModelTypeSet types_to_download,
    base::OnceCallback<void(ModelTypeSet, ModelTypeSet)> ready_task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Update the enabled types for the bridge and sync manager.
  const ModelTypeSet enabled_types = sync_manager_->GetConnectedTypes();
  DCHECK(Difference(enabled_types, ProtocolTypes()).Empty());

  const ModelTypeSet failed_types =
      Difference(types_to_download, sync_manager_->InitialSyncEndedTypes());
  const ModelTypeSet succeeded_types =
      Difference(types_to_download, failed_types);
  CHECK_EQ(
      succeeded_types,
      Intersection(types_to_download, sync_manager_->InitialSyncEndedTypes()));

  host_.Call(
      FROM_HERE, &SyncEngineImpl::FinishConfigureDataTypesOnFrontendLoop,
      enabled_types,
      base::BindOnce(std::move(ready_task), succeeded_types, failed_types));
}

void SyncEngineBackend::SendBufferedProtocolEventsAndEnableForwarding() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  forward_protocol_events_ = true;

  if (sync_manager_) {
    // Grab our own copy of the buffered events.
    // The buffer is not modified by this operation.
    std::vector<std::unique_ptr<ProtocolEvent>> buffered_events =
        sync_manager_->GetBufferedProtocolEvents();

    // Send them all over the fence to the host.
    for (std::unique_ptr<ProtocolEvent>& event : buffered_events) {
      host_.Call(FROM_HERE, &SyncEngineImpl::HandleProtocolEventOnFrontendLoop,
                 std::move(event));
    }
  }
}

void SyncEngineBackend::DisableProtocolEventForwarding() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  forward_protocol_events_ = false;
}

void SyncEngineBackend::DoOnCookieJarChanged(bool account_mismatch,
                                             base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sync_manager_->OnCookieJarChanged(account_mismatch);
  if (!callback.is_null()) {
    host_.Call(FROM_HERE, &SyncEngineImpl::OnCookieJarChangedDoneOnFrontendLoop,
               std::move(callback));
  }
}

void SyncEngineBackend::DoOnInvalidatorClientIdChange(
    const std::string& client_id) {
  sync_manager_->UpdateInvalidationClientId(client_id);
}

void SyncEngineBackend::DoOnStandaloneInvalidationReceived(
    const std::string& payload,
    const ModelTypeSet& interested_data_types) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(base::FeatureList::IsEnabled(kUseSyncInvalidations));
  const IncomingInvalidationStatus status =
      DoOnStandaloneInvalidationReceivedImpl(payload, interested_data_types);
  RecordIncomingInvalidationStatus(status);
}

SyncEngineBackend::IncomingInvalidationStatus
SyncEngineBackend::DoOnStandaloneInvalidationReceivedImpl(
    const std::string& payload,
    const ModelTypeSet& interested_data_types) {
  sync_pb::SyncInvalidationsPayload payload_message;
  if (!payload_message.ParseFromString(payload)) {
    return IncomingInvalidationStatus::kPayloadParseFailed;
  }

  bool contains_valid_model_type = false;

  std::vector<int> field_numbers;
  field_numbers.reserve(payload_message.data_type_invalidations_size());
  for (const auto& data_type_invalidation :
       payload_message.data_type_invalidations()) {
    field_numbers.push_back(data_type_invalidation.data_type_id());
  }
  for (auto model_type :
       GetModelTypeSetFromSpecificsFieldNumberList(field_numbers)) {
    if (!interested_data_types.Has(model_type)) {
      // Filter out invalidations for unsubscribed data types.
      continue;
    }

    contains_valid_model_type = true;
    RecordInvalidationPerModelType(model_type);
    absl::optional<int64_t> version;
    if (payload_message.has_version()) {
      version = payload_message.version();
    }
    std::unique_ptr<SyncInvalidation> inv_adapter =
        std::make_unique<SyncInvalidationAdapter>(payload_message.hint(),
                                                  version);
    sync_manager_->OnIncomingInvalidation(model_type, std::move(inv_adapter));
  }
  if (contains_valid_model_type) {
    return IncomingInvalidationStatus::kSuccess;
  }
  return IncomingInvalidationStatus::kUnknownModelType;
}

void SyncEngineBackend::DoOnActiveDevicesChanged(
    ActiveDevicesInvalidationInfo active_devices_invalidation_info) {
  sync_manager_->UpdateActiveDevicesInvalidationInfo(
      std::move(active_devices_invalidation_info));
}

void SyncEngineBackend::GetNigoriNodeForDebugging(AllNodesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  nigori_controller_->GetAllNodes(std::move(callback));
}

bool SyncEngineBackend::HasUnsyncedItemsForTest() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(sync_manager_);
  return sync_manager_->HasUnsyncedItemsForTest();
}

void SyncEngineBackend::LoadAndConnectNigoriController() {
  // The controller for Nigori is not exposed to the UI thread or the
  // DataTypeManager, so we need to start it here manually.
  ConfigureContext configure_context;
  configure_context.authenticated_account_id = authenticated_account_id_;
  configure_context.cache_guid = sync_manager_->cache_guid();
  // Always use kFull mode: it is actually not relevant for Nigori and switching
  // modes harder to detect on this level / can make first sync setup more
  // complicated.
  configure_context.sync_mode = SyncMode::kFull;
  configure_context.configuration_start_time = base::Time::Now();
  nigori_controller_->LoadModels(configure_context, base::DoNothing());
  DCHECK_EQ(nigori_controller_->state(), DataTypeController::MODEL_LOADED);
  sync_manager_->GetModelTypeConnector()->ConnectDataType(
      NIGORI, nigori_controller_->Connect());
}

}  // namespace syncer
