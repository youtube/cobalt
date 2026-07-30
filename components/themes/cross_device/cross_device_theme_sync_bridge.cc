// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/themes/cross_device/cross_device_theme_sync_bridge.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/sequence_checker.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/model_error.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/theme_android_specifics.pb.h"
#include "components/sync/protocol/theme_ios_specifics.pb.h"
#include "components/sync/protocol/theme_specifics.pb.h"

namespace themes {

namespace {

// Helpers to get specific proto from EntitySpecifics.
template <typename Specifics>
const Specifics& GetSpecifics(const sync_pb::EntitySpecifics& specifics);

template <>
const sync_pb::ThemeIosSpecifics& GetSpecifics<sync_pb::ThemeIosSpecifics>(
    const sync_pb::EntitySpecifics& specifics) {
  return specifics.theme_ios();
}

template <>
const sync_pb::ThemeAndroidSpecifics&
GetSpecifics<sync_pb::ThemeAndroidSpecifics>(
    const sync_pb::EntitySpecifics& specifics) {
  return specifics.theme_android();
}

}  // namespace

template <typename RemoteSpecifics, typename LocalSpecifics>
CrossDeviceThemeSyncBridge<RemoteSpecifics, LocalSpecifics>::
    CrossDeviceThemeSyncBridge(
        syncer::DataType type,
        TranslateCallback translate_cb,
        CrossDeviceThemeTracker<LocalSpecifics>* tracker,
        std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
        syncer::OnceDataTypeStoreFactory store_factory)
    : syncer::DataTypeSyncBridge(std::move(change_processor)),
      type_(type),
      translate_cb_(std::move(translate_cb)),
      tracker_(tracker) {
  DCHECK(tracker_);
  std::move(store_factory)
      .Run(type_, base::BindOnce(&CrossDeviceThemeSyncBridge::OnStoreCreated,
                                 weak_ptr_factory_.GetWeakPtr()));
}

template <typename RemoteSpecifics, typename LocalSpecifics>
CrossDeviceThemeSyncBridge<RemoteSpecifics,
                           LocalSpecifics>::~CrossDeviceThemeSyncBridge() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

template <typename RemoteSpecifics, typename LocalSpecifics>
bool CrossDeviceThemeSyncBridge<RemoteSpecifics,
                                LocalSpecifics>::IsStoreInitializedForTesting()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return store_ != nullptr;
}

template <typename RemoteSpecifics, typename LocalSpecifics>
void CrossDeviceThemeSyncBridge<RemoteSpecifics, LocalSpecifics>::
    OnSyncStarting(const syncer::DataTypeActivationRequest& request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  syncer::DataTypeSyncBridge::OnSyncStarting(request);
  tracker_->OnBridgeStatusChanged(type_, ServiceStatus::kActive);
}

template <typename RemoteSpecifics, typename LocalSpecifics>
std::unique_ptr<syncer::MetadataChangeList>
CrossDeviceThemeSyncBridge<RemoteSpecifics,
                           LocalSpecifics>::CreateMetadataChangeList() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return syncer::DataTypeStore::WriteBatch::CreateMetadataChangeList();
}

template <typename RemoteSpecifics, typename LocalSpecifics>
std::optional<syncer::ModelError>
CrossDeviceThemeSyncBridge<RemoteSpecifics, LocalSpecifics>::MergeFullSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return ApplyIncrementalSyncChanges(std::move(metadata_change_list),
                                     std::move(entity_changes));
}

template <typename RemoteSpecifics, typename LocalSpecifics>
std::optional<syncer::ModelError>
CrossDeviceThemeSyncBridge<RemoteSpecifics, LocalSpecifics>::
    ApplyIncrementalSyncChanges(
        std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
        syncer::EntityChangeList entity_changes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch =
      store_->CreateWriteBatch(std::move(metadata_change_list));

  for (const auto& change : entity_changes) {
    if (change->type() == syncer::EntityChange::ACTION_DELETE) {
      batch->DeleteData(change->storage_key());
      tracker_->RemoveThemeInfo(change->storage_key());
    } else {
      const sync_pb::EntitySpecifics& specifics = change->data().specifics;
      const RemoteSpecifics& platform_specifics =
          GetSpecifics<RemoteSpecifics>(specifics);
      DeviceThemeInfo<LocalSpecifics> theme_info =
          translate_cb_.Run(platform_specifics);

      batch->WriteData(change->storage_key(), specifics.SerializeAsString());
      tracker_->UpdateThemeInfo(change->storage_key(), std::move(theme_info));
    }
  }

  Commit(std::move(batch));
  return std::nullopt;
}

template <typename RemoteSpecifics, typename LocalSpecifics>
std::unique_ptr<syncer::DataBatch>
CrossDeviceThemeSyncBridge<RemoteSpecifics, LocalSpecifics>::GetDataForCommit(
    StorageKeyList storage_keys) {
  NOTREACHED();
}

template <typename RemoteSpecifics, typename LocalSpecifics>
std::unique_ptr<syncer::DataBatch>
CrossDeviceThemeSyncBridge<RemoteSpecifics,
                           LocalSpecifics>::GetAllDataForDebugging() {
  return nullptr;
}

template <typename RemoteSpecifics, typename LocalSpecifics>
sync_pb::EntitySpecifics
CrossDeviceThemeSyncBridge<RemoteSpecifics, LocalSpecifics>::
    TrimAllSupportedFieldsFromRemoteSpecifics(
        const sync_pb::EntitySpecifics& entity_specifics) const {
  return entity_specifics;
}

template <typename RemoteSpecifics, typename LocalSpecifics>
bool CrossDeviceThemeSyncBridge<RemoteSpecifics, LocalSpecifics>::
    IsEntityDataValid(const syncer::EntityData& entity_data) const {
  return true;
}

template <typename RemoteSpecifics, typename LocalSpecifics>
std::string
CrossDeviceThemeSyncBridge<RemoteSpecifics, LocalSpecifics>::GetClientTag(
    const syncer::EntityData& entity_data) const {
  return GetStorageKey(entity_data);
}

template <typename RemoteSpecifics, typename LocalSpecifics>
std::string
CrossDeviceThemeSyncBridge<RemoteSpecifics, LocalSpecifics>::GetStorageKey(
    const syncer::EntityData& entity_data) const {
  return entity_data.client_tag_hash.value();
}

template <typename RemoteSpecifics, typename LocalSpecifics>
void CrossDeviceThemeSyncBridge<RemoteSpecifics, LocalSpecifics>::
    ApplyDisableSyncChanges(std::unique_ptr<syncer::MetadataChangeList>
                                delete_metadata_change_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!store_) {
    return;
  }
  store_->DeleteAllDataAndMetadata(
      std::move(delete_metadata_change_list),
      base::BindOnce(&CrossDeviceThemeSyncBridge::OnDatabaseDeleted,
                     weak_ptr_factory_.GetWeakPtr()));
  tracker_->ClearAllThemeInfo();
  tracker_->OnBridgeStatusChanged(type_, ServiceStatus::kSyncDisabled);
}

template <typename RemoteSpecifics, typename LocalSpecifics>
void CrossDeviceThemeSyncBridge<RemoteSpecifics, LocalSpecifics>::
    OnStoreCreated(const std::optional<syncer::ModelError>& error,
                   std::unique_ptr<syncer::DataTypeStore> store) {
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }
  store_ = std::move(store);
  store_->ReadAllDataAndMetadata(
      base::BindOnce(&CrossDeviceThemeSyncBridge::OnReadAllDataAndMetadata,
                     weak_ptr_factory_.GetWeakPtr()));
}

template <typename RemoteSpecifics, typename LocalSpecifics>
void CrossDeviceThemeSyncBridge<RemoteSpecifics, LocalSpecifics>::
    OnReadAllDataAndMetadata(
        const std::optional<syncer::ModelError>& error,
        std::unique_ptr<syncer::DataTypeStore::RecordList> data_records,
        std::unique_ptr<syncer::MetadataBatch> metadata_batch) {
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  for (const auto& record : *data_records) {
    sync_pb::EntitySpecifics specifics;
    if (specifics.ParseFromString(record.value)) {
      const RemoteSpecifics& platform_specifics =
          GetSpecifics<RemoteSpecifics>(specifics);
      DeviceThemeInfo<LocalSpecifics> theme_info =
          translate_cb_.Run(platform_specifics);
      tracker_->UpdateThemeInfo(record.id, std::move(theme_info));
    }
  }

  change_processor()->ModelReadyToSync(std::move(metadata_batch));
}

template <typename RemoteSpecifics, typename LocalSpecifics>
void CrossDeviceThemeSyncBridge<RemoteSpecifics, LocalSpecifics>::Commit(
    std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch) {
  store_->CommitWriteBatch(
      std::move(batch),
      base::BindOnce(&CrossDeviceThemeSyncBridge::OnCommitResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

template <typename RemoteSpecifics, typename LocalSpecifics>
void CrossDeviceThemeSyncBridge<RemoteSpecifics, LocalSpecifics>::
    OnCommitResult(const std::optional<syncer::ModelError>& error) {
  if (error) {
    change_processor()->ReportError(*error);
  }
}

template <typename RemoteSpecifics, typename LocalSpecifics>
void CrossDeviceThemeSyncBridge<RemoteSpecifics, LocalSpecifics>::
    OnDatabaseDeleted(const std::optional<syncer::ModelError>& error) {
  if (error) {
    change_processor()->ReportError(*error);
  }
}

// Explicit instantiations.
template class CrossDeviceThemeSyncBridge<sync_pb::ThemeAndroidSpecifics,
                                          sync_pb::ThemeSpecifics>;
template class CrossDeviceThemeSyncBridge<sync_pb::ThemeIosSpecifics,
                                          sync_pb::ThemeSpecifics>;

}  // namespace themes
