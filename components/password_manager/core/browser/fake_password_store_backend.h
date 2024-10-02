// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FAKE_PASSWORD_STORE_BACKEND_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FAKE_PASSWORD_STORE_BACKEND_H_

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/password_store_backend.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace password_manager {

struct PasswordForm;

class SmartBubbleStatsStore;

using PasswordMap = std::
    map<std::string /* signon_realm */, std::vector<PasswordForm>, std::less<>>;

// Fake password store backend to be used in tests.
class FakePasswordStoreBackend : public PasswordStoreBackend {
 public:
  using UpdateAlwaysSucceeds =
      base::StrongAlias<struct UpdateAlwaysSucceedsTab, bool>;

  // The default Fake password store is a profile store that treats update calls
  // like the built-in backend and only updates existing credentials. If the
  // backend should behave like the Android backend which uses an underlying
  // "upsert" mechanism to create non-existing credentials, use the constructor
  // that allows to pass `UpdateAlwaysSucceeds(true)`.
  FakePasswordStoreBackend();
  explicit FakePasswordStoreBackend(
      IsAccountStore is_account_store,
      scoped_refptr<base::SequencedTaskRunner> task_runner = nullptr);
  FakePasswordStoreBackend(
      IsAccountStore is_account_store,
      UpdateAlwaysSucceeds update_always_succeeds,
      scoped_refptr<base::SequencedTaskRunner> task_runner = nullptr);
  ~FakePasswordStoreBackend() override;

  void Clear();

  const PasswordMap& stored_passwords() const { return stored_passwords_; }
  IsAccountStore is_account_store() const { return is_account_store_; }

 private:
  // Implements PasswordStoreBackend interface.
  void InitBackend(RemoteChangesReceived remote_form_changes_received,
                   base::RepeatingClosure sync_enabled_or_disabled_cb,
                   base::OnceCallback<void(bool)> completion) override;
  void Shutdown(base::OnceClosure shutdown_completed) override;
  void GetAllLoginsAsync(LoginsOrErrorReply callback) override;
  void GetAutofillableLoginsAsync(LoginsOrErrorReply callback) override;
  void GetAllLoginsForAccountAsync(absl::optional<std::string> account,
                                   LoginsOrErrorReply callback) override;
  void FillMatchingLoginsAsync(
      LoginsOrErrorReply callback,
      bool include_psl,
      const std::vector<PasswordFormDigest>& forms) override;
  void AddLoginAsync(const PasswordForm& form,
                     PasswordChangesOrErrorReply callback) override;
  void UpdateLoginAsync(const PasswordForm& form,
                        PasswordChangesOrErrorReply callback) override;
  void RemoveLoginAsync(const PasswordForm& form,
                        PasswordChangesOrErrorReply callback) override;
  void RemoveLoginsByURLAndTimeAsync(
      const base::RepeatingCallback<bool(const GURL&)>& url_filter,
      base::Time delete_begin,
      base::Time delete_end,
      base::OnceCallback<void(bool)> sync_completion,
      PasswordChangesOrErrorReply callback) override;
  void RemoveLoginsCreatedBetweenAsync(
      base::Time delete_begin,
      base::Time delete_end,
      PasswordChangesOrErrorReply callback) override;
  void DisableAutoSignInForOriginsAsync(
      const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
      base::OnceClosure completion) override;
  SmartBubbleStatsStore* GetSmartBubbleStatsStore() override;
  FieldInfoStore* GetFieldInfoStore() override;
  std::unique_ptr<syncer::ProxyModelTypeControllerDelegate>
  CreateSyncControllerDelegate() override;
  void ClearAllLocalPasswords() override;
  void OnSyncServiceInitialized(syncer::SyncService* sync_service) override;

  // Returns the task runner. Defaults to
  // `base::SequencedTaskRunner::GetCurrentDefault` if none is injected.
  const scoped_refptr<base::SequencedTaskRunner>& GetTaskRunner() const;

  LoginsResult GetAllLoginsInternal();
  LoginsResult GetAutofillableLoginsInternal();
  LoginsResult FillMatchingLoginsInternal(
      const std::vector<PasswordFormDigest>& forms,
      bool include_psl);
  LoginsResult FillMatchingLoginsHelper(const PasswordFormDigest& form,
                                        bool include_psl);
  PasswordStoreChangeList AddLoginInternal(const PasswordForm& form);
  PasswordStoreChangeList UpdateLoginInternal(const PasswordForm& form);
  void DisableAutoSignInForOriginsInternal(
      const base::RepeatingCallback<bool(const GURL&)>& origin_filter);
  PasswordStoreChangeList RemoveLoginInternal(const PasswordForm& form);

  const IsAccountStore is_account_store_{false};
  const UpdateAlwaysSucceeds update_always_succeeds_{false};

  PasswordMap stored_passwords_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FAKE_PASSWORD_STORE_BACKEND_H_
