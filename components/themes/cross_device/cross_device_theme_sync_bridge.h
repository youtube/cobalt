// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_THEMES_CROSS_DEVICE_CROSS_DEVICE_THEME_SYNC_BRIDGE_H_
#define COMPONENTS_THEMES_CROSS_DEVICE_CROSS_DEVICE_THEME_SYNC_BRIDGE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/data_type_sync_bridge.h"
#include "components/themes/cross_device/cross_device_theme_tracker.h"

namespace syncer {
class DataTypeLocalChangeProcessor;
}

namespace themes {

template <typename LocalSpecifics>
class CrossDeviceThemeTracker;

// Generic sync bridge for cross-device themes.
// It is read-only and delegates storage and updates to CrossDeviceThemeTracker.
template <typename RemoteSpecifics, typename LocalSpecifics>
class CrossDeviceThemeSyncBridge : public syncer::DataTypeSyncBridge {
 public:
  using TranslateCallback =
      base::RepeatingCallback<DeviceThemeInfo<LocalSpecifics>(
          const RemoteSpecifics&)>;

  CrossDeviceThemeSyncBridge(
      syncer::DataType type,
      TranslateCallback translate_cb,
      CrossDeviceThemeTracker<LocalSpecifics>* tracker,
      std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
      syncer::OnceDataTypeStoreFactory store_factory);

  CrossDeviceThemeSyncBridge(const CrossDeviceThemeSyncBridge&) = delete;
  CrossDeviceThemeSyncBridge& operator=(const CrossDeviceThemeSyncBridge&) =
      delete;

  ~CrossDeviceThemeSyncBridge() override;

  bool IsStoreInitializedForTesting() const;

  // DataTypeSyncBridge implementation:
  void OnSyncStarting(
      const syncer::DataTypeActivationRequest& request) override;

  std::unique_ptr<syncer::MetadataChangeList> CreateMetadataChangeList()
      override;

  std::optional<syncer::ModelError> MergeFullSyncData(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;

  std::optional<syncer::ModelError> ApplyIncrementalSyncChanges(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;

  std::unique_ptr<syncer::DataBatch> GetDataForCommit(
      StorageKeyList storage_keys) override;

  std::unique_ptr<syncer::DataBatch> GetAllDataForDebugging() override;

  sync_pb::EntitySpecifics TrimAllSupportedFieldsFromRemoteSpecifics(
      const sync_pb::EntitySpecifics& entity_specifics) const override;

  bool IsEntityDataValid(const syncer::EntityData& entity_data) const override;

  std::string GetClientTag(
      const syncer::EntityData& entity_data) const override;

  std::string GetStorageKey(
      const syncer::EntityData& entity_data) const override;

  void ApplyDisableSyncChanges(std::unique_ptr<syncer::MetadataChangeList>
                                   delete_metadata_change_list) override;

 private:
  void OnStoreCreated(const std::optional<syncer::ModelError>& error,
                      std::unique_ptr<syncer::DataTypeStore> store);

  void OnReadAllDataAndMetadata(
      const std::optional<syncer::ModelError>& error,
      std::unique_ptr<syncer::DataTypeStore::RecordList> data_records,
      std::unique_ptr<syncer::MetadataBatch> metadata_batch);

  void Commit(std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch);
  void OnCommitResult(const std::optional<syncer::ModelError>& error);
  void OnDatabaseDeleted(const std::optional<syncer::ModelError>& error);

  const syncer::DataType type_;
  const TranslateCallback translate_cb_;
  const raw_ptr<CrossDeviceThemeTracker<LocalSpecifics>> tracker_;

  std::unique_ptr<syncer::DataTypeStore> store_;
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<CrossDeviceThemeSyncBridge> weak_ptr_factory_{this};
};

}  // namespace themes

#endif  // COMPONENTS_THEMES_CROSS_DEVICE_CROSS_DEVICE_THEME_SYNC_BRIDGE_H_
