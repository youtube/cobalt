// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONSENT_AUDITOR_CONSENT_SYNC_BRIDGE_IMPL_H_
#define COMPONENTS_CONSENT_AUDITOR_CONSENT_SYNC_BRIDGE_IMPL_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/consent_auditor/consent_sync_bridge.h"
#include "components/sync/model/model_type_change_processor.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/model/model_type_sync_bridge.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace consent_auditor {

class ConsentSyncBridgeImpl : public ConsentSyncBridge,
                              public syncer::ModelTypeSyncBridge {
 public:
  ConsentSyncBridgeImpl(
      syncer::OnceModelTypeStoreFactory store_factory,
      std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor);

  ConsentSyncBridgeImpl(const ConsentSyncBridgeImpl&) = delete;
  ConsentSyncBridgeImpl& operator=(const ConsentSyncBridgeImpl&) = delete;

  ~ConsentSyncBridgeImpl() override;

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
  void ApplyDisableSyncChanges(std::unique_ptr<syncer::MetadataChangeList>
                                   delete_metadata_change_list) override;

  // ConsentSyncBridge implementation.
  void RecordConsent(
      std::unique_ptr<sync_pb::UserConsentSpecifics> specifics) override;
  base::WeakPtr<syncer::ModelTypeControllerDelegate> GetControllerDelegate()
      override;

  static std::string GetStorageKeyFromSpecificsForTest(
      const sync_pb::UserConsentSpecifics& specifics);
  std::unique_ptr<syncer::ModelTypeStore> StealStoreForTest();

 private:
  void RecordConsentImpl(
      std::unique_ptr<sync_pb::UserConsentSpecifics> specifics);
  // Record events in the deferred queue and clear the queue.
  void ProcessQueuedEvents();

  void OnStoreCreated(const absl::optional<syncer::ModelError>& error,
                      std::unique_ptr<syncer::ModelTypeStore> store);
  void OnReadAllMetadata(const absl::optional<syncer::ModelError>& error,
                         std::unique_ptr<syncer::MetadataBatch> metadata_batch);
  void OnCommit(const absl::optional<syncer::ModelError>& error);
  void OnReadData(
      DataCallback callback,
      const absl::optional<syncer::ModelError>& error,
      std::unique_ptr<syncer::ModelTypeStore::RecordList> data_records,
      std::unique_ptr<syncer::ModelTypeStore::IdList> missing_id_list);
  void OnReadAllData(
      DataCallback callback,
      const absl::optional<syncer::ModelError>& error,
      std::unique_ptr<syncer::ModelTypeStore::RecordList> data_records);

  // Resubmit all the consents persisted in the store to sync consents, which
  // were preserved when sync was disabled. This may resubmit entities that the
  // processor already knows about (i.e. with metadata), but it is allowed.
  void ReadAllDataAndResubmit();
  void OnReadAllDataToResubmit(
      const absl::optional<syncer::ModelError>& error,
      std::unique_ptr<syncer::ModelTypeStore::RecordList> data_records);

  // Persistent storage for in flight consents. Should remain quite small, as we
  // delete upon commit confirmation. May contain consents without metadata
  // (e.g. persisted when sync was disabled).
  std::unique_ptr<syncer::ModelTypeStore> store_;

  // Used to store consents while the store or change processor are not
  // ready.
  std::vector<std::unique_ptr<sync_pb::UserConsentSpecifics>>
      deferred_consents_while_initializing_;

  base::WeakPtrFactory<ConsentSyncBridgeImpl> weak_ptr_factory_{this};
};

}  // namespace consent_auditor

#endif  // COMPONENTS_CONSENT_AUDITOR_CONSENT_SYNC_BRIDGE_IMPL_H_
