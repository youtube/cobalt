// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_BACKEND_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_BACKEND_H_

#include <vector>

#include "base/functional/callback_forward.h"
#include "components/password_manager/core/browser/password_form_digest.h"
#include "components/password_manager/core/browser/password_store_backend_error.h"
#include "components/password_manager/core/browser/password_store_change.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace syncer {
class ProxyModelTypeControllerDelegate;
class SyncService;
}  // namespace syncer

class PrefService;

namespace password_manager {

struct PasswordForm;

class FieldInfoStore;
class SmartBubbleStatsStore;

using LoginsResult = std::vector<std::unique_ptr<PasswordForm>>;
using LoginsReply = base::OnceCallback<void(LoginsResult)>;

using PasswordChanges = absl::optional<PasswordStoreChangeList>;
using PasswordChangesOrError =
    absl::variant<PasswordChanges, PasswordStoreBackendError>;
using PasswordChangesOrErrorReply =
    base::OnceCallback<void(PasswordChangesOrError)>;

using LoginsResultOrError =
    absl::variant<LoginsResult, PasswordStoreBackendError>;
using LoginsOrErrorReply = base::OnceCallback<void(LoginsResultOrError)>;

// The backend is used by the `PasswordStore` to interact with the storage in a
// platform-dependent way (e.g. on Desktop, it calls a local database while on
// Android, it sends requests to a service).
// All methods are required to do their work asynchronously to prevent expensive
// IO operation from possibly blocking the main thread.
class PasswordStoreBackend {
 public:
  using RemoteChangesReceived =
      base::RepeatingCallback<void(absl::optional<PasswordStoreChangeList>)>;

  PasswordStoreBackend() = default;
  PasswordStoreBackend(const PasswordStoreBackend&) = delete;
  PasswordStoreBackend(PasswordStoreBackend&&) = delete;
  PasswordStoreBackend& operator=(const PasswordStoreBackend&) = delete;
  PasswordStoreBackend& operator=(PasswordStoreBackend&&) = delete;
  virtual ~PasswordStoreBackend() = default;

  // TODO(crbug.bom/1226042): Rename this to Init after PasswordStoreImpl no
  // longer inherits PasswordStore.
  virtual void InitBackend(RemoteChangesReceived remote_form_changes_received,
                           base::RepeatingClosure sync_enabled_or_disabled_cb,
                           base::OnceCallback<void(bool)> completion) = 0;

  // Shuts down the store asynchronously. The callback is run on the main thread
  // after the shutdown has concluded and it is safe to delete the backend.
  virtual void Shutdown(base::OnceClosure shutdown_completed) = 0;

  // Returns the complete list of PasswordForms (regardless of their blocklist
  // status). Callback is called on the main sequence.
  virtual void GetAllLoginsAsync(LoginsOrErrorReply callback) = 0;

  // Returns the complete list of non-blocklist PasswordForms. Callback is
  // called on the main sequence.
  virtual void GetAutofillableLoginsAsync(LoginsOrErrorReply callback) = 0;

  // Returns the complete list of PasswordForms (regardless of their blocklist
  // status) saved in the given sync |account|. The passed account should be a
  // current or former syncing account, otherwise |callback| will be
  // called with an error result. Callback is called on the main sequence.
  // TODO(crbug.com/1315594): Clean up/refactor to avoid having methods
  // introduced for a specific backend in this interface.
  virtual void GetAllLoginsForAccountAsync(absl::optional<std::string> account,
                                           LoginsOrErrorReply callback) = 0;

  // Returns all PasswordForms with the same signon_realm as a form in |forms|.
  // If |include_psl|==true, the PSL-matched forms are also included.
  // If multiple forms are given, those will be concatenated.
  // Callback is called on the main sequence.
  virtual void FillMatchingLoginsAsync(
      LoginsOrErrorReply callback,
      bool include_psl,
      const std::vector<PasswordFormDigest>& forms) = 0;

  // For all methods below:
  // TODO(crbug.com/1217071): Make pure virtual.
  // TODO(crbug.com/1217071): Make PasswordStoreImpl implement it like above.
  // TODO(crbug.com/1217071): Move and Update doc from PasswordStore here.
  // TODO(crbug.com/1217071): Delete corresponding Impl method from
  //  PasswordStore and the async method on backend_ instead.

  // The completion callback in each of the write operations below receive a
  // variant of optional PasswordStoreChangeList or PasswordStoreBackendError.
  // In case of success that the changelist will be populated with the executed
  // changes. The absence of the changelist indicates that the used backend
  // (e.g. on Android) cannot confirm of the execution and a re-fetch is
  // required to know the current state of the backend.
  virtual void AddLoginAsync(const PasswordForm& form,
                             PasswordChangesOrErrorReply callback) = 0;
  virtual void UpdateLoginAsync(const PasswordForm& form,
                                PasswordChangesOrErrorReply callback) = 0;
  virtual void RemoveLoginAsync(const PasswordForm& form,
                                PasswordChangesOrErrorReply callback) = 0;
  virtual void RemoveLoginsByURLAndTimeAsync(
      const base::RepeatingCallback<bool(const GURL&)>& url_filter,
      base::Time delete_begin,
      base::Time delete_end,
      base::OnceCallback<void(bool)> sync_completion,
      PasswordChangesOrErrorReply callback) = 0;
  virtual void RemoveLoginsCreatedBetweenAsync(
      base::Time delete_begin,
      base::Time delete_end,
      PasswordChangesOrErrorReply callback) = 0;
  virtual void DisableAutoSignInForOriginsAsync(
      const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
      base::OnceClosure completion) = 0;

  virtual SmartBubbleStatsStore* GetSmartBubbleStatsStore() = 0;
  virtual FieldInfoStore* GetFieldInfoStore() = 0;

  // For sync codebase only: instantiates a proxy controller delegate to
  // react to sync events.
  virtual std::unique_ptr<syncer::ProxyModelTypeControllerDelegate>
  CreateSyncControllerDelegate() = 0;

  // Clears all the passwords from the local storage.
  virtual void ClearAllLocalPasswords() = 0;

  // Propagates sync initialization event.
  virtual void OnSyncServiceInitialized(syncer::SyncService* sync_service) = 0;

  // Factory function for creating the backend. The Local backend requires the
  // provided `login_db_path` for storage and Android backend for migration
  // purposes.
  static std::unique_ptr<PasswordStoreBackend> Create(
      const base::FilePath& login_db_path,
      PrefService* prefs);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_BACKEND_H_
