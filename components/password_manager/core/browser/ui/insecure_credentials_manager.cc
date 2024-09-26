// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/insecure_credentials_manager.h"

#include <algorithm>
#include <iterator>
#include <set>
#include <string>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "components/password_manager/core/browser/insecure_credentials_table.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_list_sorter.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"
#include "components/password_manager/core/browser/ui/credential_utils.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#include "components/password_manager/core/common/password_manager_features.h"

#if !BUILDFLAG(IS_ANDROID)
#include "components/password_manager/core/browser/ui/reuse_check_utility.h"
#include "components/password_manager/core/browser/ui/weak_check_utility.h"
#endif

using password_manager::IsMuted;
using password_manager::TriggerBackendNotification;

namespace password_manager {

namespace {

bool SupportsMuteOperation(InsecureType insecure_type) {
  return (insecure_type == InsecureType::kLeaked ||
          insecure_type == InsecureType::kPhished);
}

#if !BUILDFLAG(IS_ANDROID)
base::flat_set<std::u16string> ExtractPasswords(
    const std::vector<CredentialUIEntry>& credentials) {
  return base::MakeFlatSet<std::u16string>(credentials, {},
                                           &CredentialUIEntry::password);
}

bool IsCheckForReusedPasswordsEnabled() {
#if BUILDFLAG(IS_IOS)
  // Weak and reused checks are controlled by the Password Checkup feature.
  return password_manager::features::IsPasswordCheckupEnabled();
#else
  return base::FeatureList::IsEnabled(
      password_manager::features::kPasswordManagerRedesign);
#endif
}

bool IsCheckForWeakPasswordsEnabled() {
#if BUILDFLAG(IS_IOS)
  // Weak and reused checks are controlled by the Password Checkup feature.
  return password_manager::features::IsPasswordCheckupEnabled();
#else
  return true;
#endif
}

#endif  // !BUILDFLAG(IS_ANDROID)
}  // namespace

InsecureCredentialsManager::InsecureCredentialsManager(
    SavedPasswordsPresenter* presenter,
    scoped_refptr<PasswordStoreInterface> profile_store,
    scoped_refptr<PasswordStoreInterface> account_store)
    : presenter_(presenter),
      profile_store_(std::move(profile_store)),
      account_store_(std::move(account_store)) {
  observed_saved_password_presenter_.Observe(presenter_.get());
}

InsecureCredentialsManager::~InsecureCredentialsManager() = default;

#if !BUILDFLAG(IS_ANDROID)
void InsecureCredentialsManager::StartReuseCheck(
    base::OnceClosure on_check_done) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&BulkReuseCheck, presenter_->GetSavedPasswords(),
                     presenter_->GetAffiliatedGroups()),
      base::BindOnce(&InsecureCredentialsManager::OnReuseCheckDone,
                     weak_ptr_factory_.GetWeakPtr(), base::ElapsedTimer())
          .Then(std::move(on_check_done)));
}

void InsecureCredentialsManager::StartWeakCheck(
    base::OnceClosure on_check_done) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&BulkWeakCheck,
                     ExtractPasswords(presenter_->GetSavedPasswords())),
      base::BindOnce(&InsecureCredentialsManager::OnWeakCheckDone,
                     weak_ptr_factory_.GetWeakPtr(), base::ElapsedTimer())
          .Then(std::move(on_check_done)));
}
#endif  // !BUILDFLAG(IS_ANDROID)

void InsecureCredentialsManager::SaveInsecureCredential(
    const LeakCheckCredential& leak) {
  // Iterate over all currently saved credentials and mark those as insecure
  // that have the same canonicalized username and password.
  const std::u16string canonicalized_username =
      CanonicalizeUsername(leak.username());
  for (const auto& credential : presenter_->GetSavedPasswords()) {
    if (credential.password == leak.password() &&
        CanonicalizeUsername(credential.username) == canonicalized_username &&
        !credential.password_issues.contains(InsecureType::kLeaked)) {
      CredentialUIEntry credential_to_update = credential;
      credential_to_update.password_issues.insert_or_assign(
          InsecureType::kLeaked,
          InsecurityMetadata(base::Time::Now(), IsMuted(false),
                             TriggerBackendNotification(false)));
      presenter_->EditSavedCredentials(credential, credential_to_update);
    }
  }
}

bool InsecureCredentialsManager::MuteCredential(
    const CredentialUIEntry& credential) {
  CredentialUIEntry updated_credential = credential;
  for (auto& password_issue : updated_credential.password_issues) {
    if (!password_issue.second.is_muted.value() &&
        SupportsMuteOperation(password_issue.first)) {
      password_issue.second.is_muted = IsMuted(true);
    }
  }
  return presenter_->EditSavedCredentials(credential, updated_credential) ==
         SavedPasswordsPresenter::EditResult::kSuccess;
}

bool InsecureCredentialsManager::UnmuteCredential(
    const CredentialUIEntry& credential) {
  CredentialUIEntry updated_credential = credential;
  for (auto& password_issue : updated_credential.password_issues) {
    if (password_issue.second.is_muted.value() &&
        SupportsMuteOperation(password_issue.first)) {
      password_issue.second.is_muted = IsMuted(false);
    }
  }
  return presenter_->EditSavedCredentials(credential, updated_credential) ==
         SavedPasswordsPresenter::EditResult::kSuccess;
}

std::vector<CredentialUIEntry>
InsecureCredentialsManager::GetInsecureCredentialEntries() const {
  DCHECK(presenter_);
  std::vector<CredentialUIEntry> credentials =
      presenter_->GetSavedCredentials();

#if BUILDFLAG(IS_ANDROID)
  // Otherwise erase entries which aren't leaked and phished.
  base::EraseIf(credentials, [](const auto& credential) {
    return !IsCompromised(credential);
  });
  return credentials;
#else
  for (auto& credential : credentials) {
    if (weak_passwords_.contains(credential.password)) {
      credential.password_issues.insert(
          {password_manager::InsecureType::kWeak,
           password_manager::InsecurityMetadata(
               base::Time(), IsMuted(false),
               TriggerBackendNotification(false))});
    }
    if (reused_passwords_.contains(credential.password)) {
      credential.password_issues.insert(
          {password_manager::InsecureType::kReused,
           password_manager::InsecurityMetadata(
               base::Time(), IsMuted(false),
               TriggerBackendNotification(false))});
    }
  }

  base::EraseIf(credentials, [](const auto& credential) {
    return credential.password_issues.empty();
  });
  return credentials;
#endif
}

void InsecureCredentialsManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void InsecureCredentialsManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void InsecureCredentialsManager::OnReuseCheckDone(
    base::ElapsedTimer timer_since_reuse_check_start,
    base::flat_set<std::u16string> reused_passwords) {
  base::UmaHistogramTimes("PasswordManager.ReuseCheck.Time",
                          timer_since_reuse_check_start.Elapsed());
  reused_passwords_ = std::move(reused_passwords);
  NotifyInsecureCredentialsChanged();
}

void InsecureCredentialsManager::OnWeakCheckDone(
    base::ElapsedTimer timer_since_weak_check_start,
    base::flat_set<std::u16string> weak_passwords) {
  base::UmaHistogramTimes("PasswordManager.WeakCheck.Time",
                          timer_since_weak_check_start.Elapsed());
  weak_passwords_ = std::move(weak_passwords);
  NotifyInsecureCredentialsChanged();
}

void InsecureCredentialsManager::OnEdited(const CredentialUIEntry& credential) {
  // The WeakCheck feature is not available on Android yet. Disable on Android
  // to avoid pulling in a big dependency on zxcvbn.
#if !BUILDFLAG(IS_ANDROID)
  if (IsCheckForReusedPasswordsEnabled()) {
    // Re-run reused check since user might have changed reused password.
    StartReuseCheck();
  }

  if (IsCheckForWeakPasswordsEnabled()) {
    const std::u16string& password = credential.password;
    if (weak_passwords_.contains(password) || !IsWeak(password)) {
      // Either the password is already known to be weak, or it is not weak at
      // all. In both cases there is nothing to do.
      return;
    }

    weak_passwords_.insert(password);
    NotifyInsecureCredentialsChanged();
  }
#endif
}

// Re-computes the list of insecure credentials with passwords after obtaining a
// new list of saved passwords.
void InsecureCredentialsManager::OnSavedPasswordsChanged() {
  NotifyInsecureCredentialsChanged();
}

void InsecureCredentialsManager::NotifyInsecureCredentialsChanged() {
  for (auto& observer : observers_) {
    observer.OnInsecureCredentialsChanged();
  }
}

PasswordStoreInterface& InsecureCredentialsManager::GetStoreFor(
    const PasswordForm& form) {
  return form.IsUsingAccountStore() ? *account_store_ : *profile_store_;
}

}  // namespace password_manager
