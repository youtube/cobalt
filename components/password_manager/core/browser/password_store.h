// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_H_

#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/cancelable_callback.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/types/strong_alias.h"
#include "build/build_config.h"
#include "components/password_manager/core/browser/affiliation/affiliated_match_helper.h"
#include "components/password_manager/core/browser/field_info_store.h"
#include "components/password_manager/core/browser/password_form_digest.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_store_backend.h"
#include "components/password_manager/core/browser/password_store_change.h"
#include "components/password_manager/core/browser/password_store_interface.h"
#include "components/password_manager/core/browser/password_store_sync.h"
#include "components/password_manager/core/browser/smart_bubble_stats_store.h"

class PrefService;

namespace syncer {
class ProxyModelTypeControllerDelegate;
}  // namespace syncer

namespace password_manager {

struct PasswordForm;

using IsAccountStore = base::StrongAlias<class IsAccountStoreTag, bool>;

using metrics_util::GaiaPasswordHashChange;

class PasswordStoreConsumer;
class GetLoginsWithAffiliationsRequestHandler;

// Used to notify that unsynced credentials are about to be deleted.
class UnsyncedCredentialsDeletionNotifier {
 public:
  // Should be called from the UI thread.
  virtual void Notify(std::vector<PasswordForm>) = 0;
  virtual ~UnsyncedCredentialsDeletionNotifier() = default;
  virtual base::WeakPtr<UnsyncedCredentialsDeletionNotifier> GetWeakPtr() = 0;
};

// Partial, cross-platform implementation for storing form passwords.
// The login request/manipulation API is not threadsafe and must be used
// from the UI thread.
// PasswordStoreSync is a hidden base class because only PasswordSyncBridge
// needs to access these methods.
class PasswordStore : public PasswordStoreInterface {
 public:
  explicit PasswordStore(std::unique_ptr<PasswordStoreBackend> backend);

  PasswordStore(const PasswordStore&) = delete;
  PasswordStore& operator=(const PasswordStore&) = delete;

  // Always call this too on the UI thread.
  // TODO(crbug.com/1218413): Move initialization into the core interface, too.
  void Init(PrefService* prefs,
            std::unique_ptr<AffiliatedMatchHelper> affiliated_match_helper);

  // RefcountedKeyedService:
  void ShutdownOnUIThread() override;

  // PasswordStoreInterface:
  bool IsAbleToSavePasswords() const override;
  void AddLogin(const PasswordForm& form,
                base::OnceClosure completion = base::DoNothing()) override;
  void AddLogins(const std::vector<PasswordForm>& forms,
                 base::OnceClosure completion = base::DoNothing()) override;
  void UpdateLogin(const PasswordForm& form,
                   base::OnceClosure completion = base::DoNothing()) override;
  void UpdateLogins(const std::vector<PasswordForm>& forms,
                    base::OnceClosure completion = base::DoNothing()) override;
  void UpdateLoginWithPrimaryKey(
      const PasswordForm& new_form,
      const PasswordForm& old_primary_key,
      base::OnceClosure completion = base::DoNothing()) override;
  void RemoveLogin(const PasswordForm& form) override;
  void RemoveLoginsByURLAndTime(
      const base::RepeatingCallback<bool(const GURL&)>& url_filter,
      base::Time delete_begin,
      base::Time delete_end,
      base::OnceClosure completion = base::NullCallback(),
      base::OnceCallback<void(bool)> sync_completion =
          base::NullCallback()) override;
  void RemoveLoginsCreatedBetween(base::Time delete_begin,
                                  base::Time delete_end,
                                  base::OnceCallback<void(bool)> completion =
                                      base::NullCallback()) override;
  void DisableAutoSignInForOrigins(
      const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
      base::OnceClosure completion = base::NullCallback()) override;
  void Unblocklist(
      const PasswordFormDigest& form_digest,
      base::OnceClosure completion = base::NullCallback()) override;
  void GetLogins(const PasswordFormDigest& form,
                 base::WeakPtr<PasswordStoreConsumer> consumer) override;
  void GetAutofillableLogins(
      base::WeakPtr<PasswordStoreConsumer> consumer) override;
  void GetAllLogins(base::WeakPtr<PasswordStoreConsumer> consumer) override;
  void GetAllLoginsWithAffiliationAndBrandingInformation(
      base::WeakPtr<PasswordStoreConsumer> consumer) override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  SmartBubbleStatsStore* GetSmartBubbleStatsStore() override;
  FieldInfoStore* GetFieldInfoStore() override;
  std::unique_ptr<syncer::ProxyModelTypeControllerDelegate>
  CreateSyncControllerDelegate() override;
  void OnSyncServiceInitialized(syncer::SyncService* sync_service) override;
  base::CallbackListSubscription AddSyncEnabledOrDisabledCallback(
      base::RepeatingClosure sync_enabled_or_disabled_cb) override;
  PasswordStoreBackend* GetBackendForTesting() override;

 protected:
  friend class base::RefCountedThreadSafe<PasswordStore>;

  ~PasswordStore() override;

 private:
  // Status of PasswordStore::Init().
  enum class InitStatus {
    // Initialization status is still not determined (init hasn't started or
    // finished yet).
    kUnknown,
    // Initialization is successfully finished.
    kSuccess,
    // There was an error during initialization and PasswordStore is not ready
    // to save or get passwords.
    // Removing passwords may still work.
    kFailure,
  };

  // Represents different triggers that may require requesting all logins from
  // the password store. Entries should not be renumbered and numeric values
  // should never be reused. Always keep this enum in sync with the
  // corresponding LoginsChangedTrigger in enums.xml.
  enum class LoginsChangedTrigger {
    ExternalUpdate = 0,
    Addition = 1,
    Update = 2,
    Deletion = 3,
    BatchDeletion = 4,
    Unblocklisting = 5,
    // Must be last.
    kMaxValue = Unblocklisting,
  };

  // Called on the main thread after initialization is completed.
  // |success| is true if initialization was successful. Sets the
  // |init_status_|.
  void OnInitCompleted(bool success);

  // Notifies observers that password store data may have been changed. If
  // available, it forwards the changes to observers. Otherwise, all logins are
  // requested and forwarded to `NotifyLoginsRetainedOnMainSequence`.
  void NotifyLoginsChangedOnMainSequence(
      LoginsChangedTrigger change_event,
      absl::optional<PasswordStoreChangeList> changes);

  // Notifies observers with all logins remaining after a modifying operation.
  void NotifyLoginsRetainedOnMainSequence(LoginsResultOrError result);

  // Called when the backend reports that sync has been enabled or disabled.
  void NotifySyncEnabledOrDisabledOnMainSequence();

  // The following methods notify observers that the password store may have
  // been modified via NotifyLoginsChangedOnMainSequence(). Note that there is
  // no guarantee that the called method will actually modify the password store
  // data.
  void UnblocklistInternal(base::OnceClosure completion,
                           std::vector<std::unique_ptr<PasswordForm>> forms);

  // Retrieves logins for `form` as well as for the affiliated realms if
  // such realms/logins exist. `request_handler` is responsible for combining
  // the responses from the two requests and passing them on.
  void GetLoginsForFormAndForAffiliatedRealms(
      const PasswordFormDigest& form,
      scoped_refptr<GetLoginsWithAffiliationsRequestHandler> request_handler);

  // If |forms_or_error| contains forms, it retrieves and fills in affiliation
  // and branding information for Android credentials in the forms and invokes
  // |callback| with the result. If an error was received instead, it directly
  // invokes |callback| with it, as no forms could be fetched. Called on
  // the main sequence.
  void InjectAffiliationAndBrandingInformation(
      LoginsOrErrorReply callback,
      LoginsResultOrError forms_or_error);

  // This member is called to perform the actual interaction with the storage.
  // The backend is injected via the public constructor, this member owns the
  // instance and deletes it by calling PasswordStoreBackend::Shutdown on it.
  std::unique_ptr<PasswordStoreBackend> backend_;

  // TaskRunner for tasks that run on the main sequence (usually the UI thread).
  // TODO(crbug.com/1217071): Move into backend_.
  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;

  // See PasswordStoreInterface::AddSyncEnabledOrDisabledCallback(). Wrapped in
  // unique_ptr so it can be destroyed earlier, in ShutdownOnUIThread(),
  // cancelling the existing callbacks.
  std::unique_ptr<base::RepeatingClosureList> sync_enabled_or_disabled_cbs_ =
      std::make_unique<base::RepeatingClosureList>();

  // The observers.
  base::ObserverList<Observer, /*check_empty=*/true> observers_;

  std::unique_ptr<AffiliatedMatchHelper> affiliated_match_helper_;

  raw_ptr<PrefService, DanglingUntriaged> prefs_ = nullptr;

  InitStatus init_status_ = InitStatus::kUnknown;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_H_
