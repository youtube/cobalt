// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/accounts_mutator_impl.h"

#include "build/chromeos_buildflags.h"
#include "components/prefs/pref_service.h"
#include "components/signin/internal/identity_manager/account_tracker_service.h"
#include "components/signin/internal/identity_manager/primary_account_manager.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service.h"
#include "components/signin/public/base/device_id_helper.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_constants.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace signin {

AccountsMutatorImpl::AccountsMutatorImpl(
    ProfileOAuth2TokenService* token_service,
    AccountTrackerService* account_tracker_service,
    PrimaryAccountManager* primary_account_manager,
    PrefService* pref_service)
    : token_service_(token_service),
      account_tracker_service_(account_tracker_service),
      primary_account_manager_(primary_account_manager) {
  DCHECK(token_service_);
  DCHECK(account_tracker_service_);
  DCHECK(primary_account_manager_);
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  pref_service_ = pref_service;
  DCHECK(pref_service_);
#endif
}

AccountsMutatorImpl::~AccountsMutatorImpl() {}

CoreAccountId AccountsMutatorImpl::AddOrUpdateAccount(
    const std::string& gaia_id,
    const std::string& email,
    const std::string& refresh_token,
    bool is_under_advanced_protection,
    signin_metrics::SourceForRefreshTokenOperation source) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  NOTREACHED();
#endif
  CoreAccountId account_id =
      account_tracker_service_->SeedAccountInfo(gaia_id, email);
  account_tracker_service_->SetIsAdvancedProtectionAccount(
      account_id, is_under_advanced_protection);

  // Flush the account changes to disk. Otherwise, in case of a browser crash,
  // the account may be added to the token service but not to the account
  // tracker, which is not intended.
  account_tracker_service_->CommitPendingAccountChanges();

  token_service_->UpdateCredentials(account_id, refresh_token, source);

  return account_id;
}

void AccountsMutatorImpl::UpdateAccountInfo(
    const CoreAccountId& account_id,
    Tribool is_child_account,
    Tribool is_under_advanced_protection) {
  // kUnknown is used by callers when they do not want to update the value.
  if (is_child_account != Tribool::kUnknown) {
    account_tracker_service_->SetIsChildAccount(
        account_id, is_child_account == Tribool::kTrue);
  }

  if (is_under_advanced_protection != Tribool::kUnknown) {
    account_tracker_service_->SetIsAdvancedProtectionAccount(
        account_id, is_under_advanced_protection == Tribool::kTrue);
  }
}

void AccountsMutatorImpl::RemoveAccount(
    const CoreAccountId& account_id,
    signin_metrics::SourceForRefreshTokenOperation source) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  NOTREACHED();
#endif
  token_service_->RevokeCredentials(account_id, source);
}

void AccountsMutatorImpl::RemoveAllAccounts(
    signin_metrics::SourceForRefreshTokenOperation source) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  NOTREACHED();
#endif
  token_service_->RevokeAllCredentials(source);
}

void AccountsMutatorImpl::InvalidateRefreshTokenForPrimaryAccount(
    signin_metrics::SourceForRefreshTokenOperation source) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  NOTREACHED();
#endif
  DCHECK(primary_account_manager_->HasPrimaryAccount(ConsentLevel::kSignin));
  CoreAccountInfo primary_account_info =
      primary_account_manager_->GetPrimaryAccountInfo(ConsentLevel::kSignin);
  AddOrUpdateAccount(primary_account_info.gaia, primary_account_info.email,
                     GaiaConstants::kInvalidRefreshToken,
                     primary_account_info.is_under_advanced_protection, source);
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
void AccountsMutatorImpl::MoveAccount(AccountsMutator* target,
                                      const CoreAccountId& account_id) {
  AccountInfo account_info =
      account_tracker_service_->GetAccountInfo(account_id);
  DCHECK(!account_info.account_id.empty());

  auto* target_impl = static_cast<AccountsMutatorImpl*>(target);
  target_impl->account_tracker_service_->SeedAccountInfo(account_info);
  token_service_->ExtractCredentials(target_impl->token_service_, account_id);

  // Reset the device ID from the source mutator: the exported token is linked
  // to the device ID of the current mutator on the server. Reset the device ID
  // of the current mutator to avoid tying it with the new mutator. See
  // https://crbug.com/813928#c16
  RecreateSigninScopedDeviceId(pref_service_);
}
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
CoreAccountId AccountsMutatorImpl::SeedAccountInfo(const std::string& gaia_id,
                                                   const std::string& email) {
  return account_tracker_service_->SeedAccountInfo(gaia_id, email);
}
#endif

}  // namespace signin
