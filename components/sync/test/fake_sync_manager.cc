// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/fake_sync_manager.h"

#include <cstddef>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "components/sync/engine/engine_components_factory.h"
#include "components/sync/engine/net/http_post_provider_factory.h"
#include "components/sync/test/fake_model_type_connector.h"

class GURL;

namespace syncer {

FakeSyncManager::FakeSyncManager(ModelTypeSet initial_sync_ended_types,
                                 ModelTypeSet progress_marker_types,
                                 ModelTypeSet configure_fail_types)
    : initial_sync_ended_types_(initial_sync_ended_types),
      progress_marker_types_(progress_marker_types),
      configure_fail_types_(configure_fail_types),
      last_configure_reason_(CONFIGURE_REASON_UNKNOWN) {}

FakeSyncManager::~FakeSyncManager() = default;

ModelTypeSet FakeSyncManager::GetAndResetDownloadedTypes() {
  ModelTypeSet downloaded_types = downloaded_types_;
  downloaded_types_.Clear();
  return downloaded_types;
}

ConfigureReason FakeSyncManager::GetAndResetConfigureReason() {
  ConfigureReason reason = last_configure_reason_;
  last_configure_reason_ = CONFIGURE_REASON_UNKNOWN;
  return reason;
}

int FakeSyncManager::GetInvalidationCount(ModelType type) const {
  auto it = num_invalidations_received_.find(type);
  if (it == num_invalidations_received_.end()) {
    return 0;
  }
  return it->second;
}

void FakeSyncManager::WaitForSyncThread() {
  // Post a task to |sync_task_runner_| and block until it runs.
  base::RunLoop run_loop;
  if (!sync_task_runner_->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                           run_loop.QuitClosure())) {
    NOTREACHED();
  }
  run_loop.Run();
}

void FakeSyncManager::Init(InitArgs* args) {
  sync_task_runner_ = base::SequencedTaskRunner::GetCurrentDefault();
  cache_guid_ = args->cache_guid;
  birthday_ = args->birthday;
  bag_of_chips_ = args->bag_of_chips;
}

ModelTypeSet FakeSyncManager::InitialSyncEndedTypes() {
  return initial_sync_ended_types_;
}

ModelTypeSet FakeSyncManager::GetConnectedTypes() {
  return progress_marker_types_;
}

void FakeSyncManager::UpdateCredentials(const SyncCredentials& credentials) {
  NOTIMPLEMENTED();
}

void FakeSyncManager::InvalidateCredentials() {
  NOTIMPLEMENTED();
}

void FakeSyncManager::StartSyncingNormally(base::Time last_poll_time) {
  // Do nothing.
}

void FakeSyncManager::StartConfiguration() {
  // Do nothing.
}

void FakeSyncManager::ConfigureSyncer(ConfigureReason reason,
                                      ModelTypeSet to_download,
                                      SyncFeatureState sync_feature_state,
                                      base::OnceClosure ready_task) {
  last_configure_reason_ = reason;
  ModelTypeSet success_types = to_download;
  success_types.RemoveAll(configure_fail_types_);

  DVLOG(1) << "Faking configuration. Downloading: "
           << ModelTypeSetToDebugString(success_types);

  // Now simulate the actual configuration for those types that successfully
  // download + apply.
  progress_marker_types_.PutAll(success_types);
  initial_sync_ended_types_.PutAll(success_types);
  downloaded_types_.PutAll(success_types);

  std::move(ready_task).Run();
}

void FakeSyncManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeSyncManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void FakeSyncManager::ShutdownOnSyncThread() {
  DCHECK(sync_task_runner_->RunsTasksInCurrentSequence());
}

ModelTypeConnector* FakeSyncManager::GetModelTypeConnector() {
  return &fake_model_type_connector_;
}

std::unique_ptr<ModelTypeConnector>
FakeSyncManager::GetModelTypeConnectorProxy() {
  return std::make_unique<FakeModelTypeConnector>();
}

std::string FakeSyncManager::cache_guid() {
  return cache_guid_;
}

std::string FakeSyncManager::birthday() {
  return birthday_;
}

std::string FakeSyncManager::bag_of_chips() {
  return bag_of_chips_;
}

bool FakeSyncManager::HasUnsyncedItemsForTest() {
  NOTIMPLEMENTED();
  return false;
}

SyncEncryptionHandler* FakeSyncManager::GetEncryptionHandler() {
  return &fake_encryption_handler_;
}

std::vector<std::unique_ptr<ProtocolEvent>>
FakeSyncManager::GetBufferedProtocolEvents() {
  return std::vector<std::unique_ptr<ProtocolEvent>>();
}

void FakeSyncManager::RefreshTypes(ModelTypeSet types) {
  last_refresh_request_types_ = types;
}

void FakeSyncManager::OnIncomingInvalidation(
    ModelType type,
    std::unique_ptr<SyncInvalidation> invalidation) {
  num_invalidations_received_[type]++;
}

ModelTypeSet FakeSyncManager::GetLastRefreshRequestTypes() {
  return last_refresh_request_types_;
}

void FakeSyncManager::SetInvalidatorEnabled(bool invalidator_enabled) {
  invalidator_enabled_ = invalidator_enabled;
}

void FakeSyncManager::OnCookieJarChanged(bool account_mismatch) {}

void FakeSyncManager::UpdateInvalidationClientId(const std::string&) {
  NOTIMPLEMENTED();
}

void FakeSyncManager::UpdateActiveDevicesInvalidationInfo(
    ActiveDevicesInvalidationInfo active_devices_invalidation_info) {
  // Do nothing.
}

}  // namespace syncer
