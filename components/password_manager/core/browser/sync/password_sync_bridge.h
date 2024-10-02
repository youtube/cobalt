// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SYNC_PASSWORD_SYNC_BRIDGE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SYNC_PASSWORD_SYNC_BRIDGE_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "components/password_manager/core/browser/password_store_change.h"
#include "components/password_manager/core/browser/password_store_sync.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/model_type_sync_bridge.h"

namespace syncer {
class MetadataChangeList;
class ModelTypeChangeProcessor;
}  // namespace syncer

namespace password_manager {

class PasswordStoreSync;

// Sync bridge implementation for PASSWORDS model type. Takes care of
// propagating local passwords to other clients and vice versa.
//
// This is achieved by implementing the interface ModelTypeSyncBridge, which
// ClientTagBasedModelTypeProcessor will use to interact, ultimately, with the
// sync server. See
// https://www.chromium.org/developers/design-documents/sync/model-api/#implementing-modeltypesyncbridge
// for details.
class PasswordSyncBridge : public syncer::ModelTypeSyncBridge {
 public:
  // |password_store_sync| must not be null and must outlive this object.
  PasswordSyncBridge(
      std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor,
      PasswordStoreSync* password_store_sync,
      const base::RepeatingClosure& sync_enabled_or_disabled_cb);

  PasswordSyncBridge(const PasswordSyncBridge&) = delete;
  PasswordSyncBridge& operator=(const PasswordSyncBridge&) = delete;

  ~PasswordSyncBridge() override;

  // Notifies the bridge of changes to the password database. Callers are
  // responsible for calling this function within the very same transaction as
  // the data changes.
  void ActOnPasswordStoreChanges(const PasswordStoreChangeList& changes);

  // ModelTypeSyncBridge implementation.
  std::unique_ptr<syncer::MetadataChangeList> CreateMetadataChangeList()
      override;
  absl::optional<syncer::ModelError> MergeFullSyncData(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_data) override;
  absl::optional<syncer::ModelError> ApplyIncrementalSyncChanges(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
  void GetData(StorageKeyList storage_keys, DataCallback callback) override;
  void GetAllDataForDebugging(DataCallback callback) override;
  std::string GetClientTag(const syncer::EntityData& entity_data) override;
  std::string GetStorageKey(const syncer::EntityData& entity_data) override;
  bool SupportsGetStorageKey() const override;
  void ApplyDisableSyncChanges(std::unique_ptr<syncer::MetadataChangeList>
                                   delete_metadata_change_list) override;
  sync_pb::EntitySpecifics TrimAllSupportedFieldsFromRemoteSpecifics(
      const sync_pb::EntitySpecifics& entity_specifics) const override;

  static std::string ComputeClientTagForTesting(
      const sync_pb::PasswordSpecificsData& password_data);

 private:
  // On MacOS or Linux it may happen that some passwords cannot be decrypted due
  // to modification of encryption key in Keychain or Keyring
  // (https://crbug.com/730625). This method deletes those logins from the
  // store. So during merge, the data in sync will be added to the password
  // store. This should be called during MergeFullSyncData().
  absl::optional<syncer::ModelError> CleanupPasswordStore();

  // Retrieves the storage keys of all unsynced passwords in the store.
  std::set<FormPrimaryKey> GetUnsyncedPasswordsStorageKeys();

  // If available, returns cached possibly trimmed PasswordSpecificsData for
  // given |storage_key|. By default, empty PasswordSpecificsData is returned.
  const sync_pb::PasswordSpecificsData& GetPossiblyTrimmedPasswordSpecificsData(
      const std::string& storage_key);

  // Checks whether any password entity on `metadata_map` persists specifics
  // fields in cache that are supported in the current browser version.
  bool SyncMetadataCacheContainsSupportedFields(
      const syncer::EntityMetadataMap& metadata_map) const;

  // Password store responsible for persistence.
  const raw_ptr<PasswordStoreSync> password_store_sync_;

  base::RepeatingClosure sync_enabled_or_disabled_cb_;

  // True if processing remote sync changes is in progress. Used to ignore
  // password store changes notifications while processing remote sync changes.
  bool is_processing_remote_sync_changes_ = false;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SYNC_PASSWORD_SYNC_BRIDGE_H_
