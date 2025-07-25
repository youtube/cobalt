// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNC_MODEL_IOS_CHROME_SYNC_CLIENT_H__
#define IOS_CHROME_BROWSER_SYNC_MODEL_IOS_CHROME_SYNC_CLIENT_H__

#include <memory>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/browser_sync/browser_sync_client.h"
#include "components/trusted_vault/trusted_vault_client.h"

class ChromeBrowserState;

namespace autofill {
class AutofillWebDataService;
}

namespace password_manager {
class PasswordStoreInterface;
}

namespace browser_sync {
class LocalDataQueryHelper;
class LocalDataMigrationHelper;
class SyncApiComponentFactoryImpl;
}  // namespace browser_sync

class IOSChromeSyncClient : public browser_sync::BrowserSyncClient {
 public:
  explicit IOSChromeSyncClient(ChromeBrowserState* browser_state);

  IOSChromeSyncClient(const IOSChromeSyncClient&) = delete;
  IOSChromeSyncClient& operator=(const IOSChromeSyncClient&) = delete;

  ~IOSChromeSyncClient() override;

  // BrowserSyncClient implementation.
  PrefService* GetPrefService() override;
  signin::IdentityManager* GetIdentityManager() override;
  base::FilePath GetLocalSyncBackendFolder() override;
  syncer::ModelTypeStoreService* GetModelTypeStoreService() override;
  syncer::DeviceInfoSyncService* GetDeviceInfoSyncService() override;
  favicon::FaviconService* GetFaviconService() override;
  history::HistoryService* GetHistoryService() override;
  ReadingListModel* GetReadingListModel() override;
  send_tab_to_self::SendTabToSelfSyncService* GetSendTabToSelfSyncService()
      override;
  sync_preferences::PrefServiceSyncable* GetPrefServiceSyncable() override;
  sync_sessions::SessionSyncService* GetSessionSyncService() override;
  password_manager::PasswordReceiverService* GetPasswordReceiverService()
      override;
  password_manager::PasswordSenderService* GetPasswordSenderService() override;
  syncer::DataTypeController::TypeVector CreateDataTypeControllers(
      syncer::SyncService* sync_service) override;
  syncer::SyncInvalidationsService* GetSyncInvalidationsService() override;
  trusted_vault::TrustedVaultClient* GetTrustedVaultClient() override;
  scoped_refptr<syncer::ExtensionsActivity> GetExtensionsActivity() override;
  base::WeakPtr<syncer::ModelTypeControllerDelegate>
  GetControllerDelegateForModelType(syncer::ModelType type) override;
  syncer::SyncApiComponentFactory* GetSyncApiComponentFactory() override;
  syncer::SyncTypePreferenceProvider* GetPreferenceProvider() override;
  void OnLocalSyncTransportDataCleared() override;
  void GetLocalDataDescriptions(
      syncer::ModelTypeSet types,
      base::OnceCallback<void(
          std::map<syncer::ModelType, syncer::LocalDataDescription>)> callback)
      override;
  void TriggerLocalDataMigration(syncer::ModelTypeSet types) override;

 private:
  ChromeBrowserState* const browser_state_;

  // The sync api component factory in use by this client.
  std::unique_ptr<browser_sync::SyncApiComponentFactoryImpl> component_factory_;

  // Members that must be fetched on the UI thread but accessed on their
  // respective backend threads.
  scoped_refptr<autofill::AutofillWebDataService> profile_web_data_service_;
  scoped_refptr<autofill::AutofillWebDataService> account_web_data_service_;
  scoped_refptr<password_manager::PasswordStoreInterface>
      profile_password_store_;
  scoped_refptr<password_manager::PasswordStoreInterface>
      account_password_store_;

  // The task runner for the `web_data_service_`, if any.
  scoped_refptr<base::SequencedTaskRunner> db_thread_;

  std::unique_ptr<browser_sync::LocalDataQueryHelper> local_data_query_helper_;
  std::unique_ptr<browser_sync::LocalDataMigrationHelper>
      local_data_migration_helper_;
};

#endif  // IOS_CHROME_BROWSER_SYNC_MODEL_IOS_CHROME_SYNC_CLIENT_H__
