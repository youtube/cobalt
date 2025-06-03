// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_REUSE_MANAGER_IMPL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_REUSE_MANAGER_IMPL_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_reuse_manager.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/password_manager/core/browser/password_store_interface.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace password_manager {

class PasswordReuseManagerImpl : public PasswordReuseManager,
                                 public PasswordStoreConsumer,
                                 public PasswordStoreInterface::Observer {
 public:
  PasswordReuseManagerImpl();
  ~PasswordReuseManagerImpl() override;

  // Immediately called after |Init()| to retrieve password hash data for
  // reuse detection.
  // TODO(crbug.com/1469280): This might need to be called from all platforms,
  // including ios.
  void PreparePasswordHashData(
      metrics_util::SignInState sign_in_state_for_metrics);

  // Implements KeyedService interface.
  void Shutdown() override;

  // Implements PasswordReuseManager interface.
  void Init(PrefService* prefs,
            PasswordStoreInterface* profile_store,
            PasswordStoreInterface* account_store) override;
  void ReportMetrics(const std::string& username,
                     bool is_under_advanced_protection) override;
  void CheckReuse(const std::u16string& input,
                  const std::string& domain,
                  PasswordReuseDetectorConsumer* consumer) override;
  void SaveGaiaPasswordHash(
      const std::string& username,
      const std::u16string& password,
      bool is_sync_password_for_metrics,
      metrics_util::GaiaPasswordHashChange event) override;
  void SaveEnterprisePasswordHash(const std::string& username,
                                  const std::u16string& password) override;
  void SaveSyncPasswordHash(
      const PasswordHashData& sync_password_data,
      metrics_util::GaiaPasswordHashChange event) override;
  void ClearGaiaPasswordHash(const std::string& username) override;
  void ClearAllGaiaPasswordHash() override;
  void ClearAllEnterprisePasswordHash() override;
  void ClearAllNonGmailPasswordHash() override;
  base::CallbackListSubscription RegisterStateCallbackOnHashPasswordManager(
      const base::RepeatingCallback<void(const std::string& username)>&
          callback) override;
  void SetPasswordStoreSigninNotifier(
      std::unique_ptr<PasswordStoreSigninNotifier> notifier) override;
  void ScheduleEnterprisePasswordURLUpdate() override;

 private:
  // Schedules the update of password hashes used by reuse detector.
  // |sign_in_state_for_metrics|, if not nullopt, is used for metrics only.
  void SchedulePasswordHashUpdate(
      absl::optional<metrics_util::SignInState> sign_in_state_for_metrics);

  // Executed deferred on Android in order avoid high startup latencies.
  void RequestLoginsFromStores();

  // Implements PasswordStoreConsumer interface.
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<PasswordForm>> results) override;

  // Implements PasswordStoreInterface::Observer
  void OnLoginsChanged(
      password_manager::PasswordStoreInterface* store,
      const password_manager::PasswordStoreChangeList& changes) override;
  void OnLoginsRetained(
      PasswordStoreInterface* store,
      const std::vector<PasswordForm>& retained_passwords) override;

  // Saves |username| and a hash of |password| for password reuse checking.
  // |is_gaia_password| indicates if it is a Gaia account. |event| is used for
  // metric logging. |is_sync_password_for_metrics| is whether account belong to
  // the password is a primary account with sync the feature turned on.
  void SaveProtectedPasswordHash(const std::string& username,
                                 const std::u16string& password,
                                 bool is_sync_password_for_metrics,
                                 bool is_gaia_password,
                                 metrics_util::GaiaPasswordHashChange event);

  // Schedules the given |task| to be run on the 'background_task_runner_'.
  bool ScheduleTask(base::OnceClosure task);

  // Clears existing cached passwords stored on the account store and schedules
  // a request to re-fetch.
  void AccountStoreStateChanged();

  // TaskRunner for tasks that run on the main sequence (the UI thread).
  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;

  // TaskRunner for all the background operations.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  raw_ptr<PrefService> prefs_ = nullptr;

  scoped_refptr<PasswordStoreInterface> profile_store_;

  scoped_refptr<PasswordStoreInterface> account_store_;

  // Return value of PasswordStoreInterface::AddSyncEnabledOrDisabledCallback().
  base::CallbackListSubscription account_store_cb_list_subscription_;

  // The `reuse_detector_`, owned by this PasswordReuseManager instance, but
  // living on the background thread. It will be deleted asynchronously during
  // shutdown on the background thread, so it will outlive `this` along with all
  // its in-flight tasks.
  std::unique_ptr<PasswordReuseDetector> reuse_detector_;

  // Notifies PasswordReuseManager about sign-in events.
  std::unique_ptr<PasswordStoreSigninNotifier> notifier_;

  // Responsible for saving, clearing, retrieving and encryption of a password
  // hash data in preferences.
  HashPasswordManager hash_password_manager_;

  base::WeakPtrFactory<PasswordReuseManagerImpl> weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_REUSE_MANAGER_IMPL_H_
