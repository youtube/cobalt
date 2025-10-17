// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_VALUABLES_VALUABLE_SYNC_BRIDGE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_VALUABLES_VALUABLE_SYNC_BRIDGE_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/supports_user_data.h"
#include "components/autofill/core/browser/webdata/autofill_sync_metadata_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_backend.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_observer.h"
#include "components/autofill/core/browser/webdata/valuables/valuables_table.h"
#include "components/sync/model/data_type_local_change_processor.h"
#include "components/sync/model/data_type_sync_bridge.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/protocol/entity_data.h"

namespace autofill {

class AutofillWebDataService;

class ValuableSyncBridge : public base::SupportsUserData::Data,
                           public syncer::DataTypeSyncBridge {
 public:
  ValuableSyncBridge(
      std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
      AutofillWebDataBackend* backend);
  ~ValuableSyncBridge() override;

  ValuableSyncBridge(const ValuableSyncBridge&) = delete;
  ValuableSyncBridge& operator=(const ValuableSyncBridge&) = delete;

  static void CreateForWebDataServiceAndBackend(
      AutofillWebDataBackend* web_data_backend,
      AutofillWebDataService* web_data_service);

  static syncer::DataTypeSyncBridge* FromWebDataService(
      AutofillWebDataService* web_data_service);

  // syncer::DataTypeSyncBridge implementation.
  bool SupportsIncrementalUpdates() const override;
  std::unique_ptr<syncer::MetadataChangeList> CreateMetadataChangeList()
      override;
  std::optional<syncer::ModelError> MergeFullSyncData(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_data) override;
  std::optional<syncer::ModelError> ApplyIncrementalSyncChanges(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
  std::unique_ptr<syncer::DataBatch> GetDataForCommit(
      StorageKeyList storage_keys) override;
  std::unique_ptr<syncer::DataBatch> GetAllDataForDebugging() override;
  bool IsEntityDataValid(const syncer::EntityData& entity_data) const override;
  std::string GetClientTag(
      const syncer::EntityData& entity_data) const override;
  std::string GetStorageKey(
      const syncer::EntityData& entity_data) const override;
  void ApplyDisableSyncChanges(std::unique_ptr<syncer::MetadataChangeList>
                                   delete_metadata_change_list) override;
  sync_pb::EntitySpecifics TrimAllSupportedFieldsFromRemoteSpecifics(
      const sync_pb::EntitySpecifics& entity_specifics) const override;

 private:
  // Synchronously load sync metadata from the `ValuablesTable` and pass it to
  // the processor.
  void LoadMetadata();

  // Sets the Wallet data from `entity_data` to this client and records metrics
  // about added/deleted data. Returns a ModelError if any errors occured.
  std::optional<syncer::ModelError> SetSyncData(
      const syncer::EntityChangeList& entity_data);

  bool SyncMetadataCacheContainsSupportedFields(
      const syncer::EntityMetadataMap& metadata_map) const;

  // Returns the `ValuablesTable` associated with the `web_data_backend_`.
  ValuablesTable* GetValuablesTable();

  AutofillSyncMetadataTable* GetSyncMetadataStore();

  // Queries all loyalty cards from `GetValuablesTable()`.
  // These cards are converted to their `AutofillLoyaltyCardSpecifics`
  // representation and returned as a `syncer::MutableDataBatch`.
  std::unique_ptr<syncer::MutableDataBatch> GetData();

  // The bridge should be used on the same sequence where it has been
  // constructed.
  SEQUENCE_CHECKER(sequence_checker_);

  // ValuableSyncBridge is owned by `web_data_backend_` through
  // SupportsUserData, so it's guaranteed to outlive `this`.
  const raw_ptr<AutofillWebDataBackend> web_data_backend_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_VALUABLES_VALUABLE_SYNC_BRIDGE_H_
