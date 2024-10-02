// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/accounts_cookie_mutator_impl.h"

#include <utility>
#include <vector>

#include "build/build_config.h"
#include "components/signin/internal/identity_manager/account_tracker_service.h"
#include "components/signin/internal/identity_manager/gaia_cookie_manager_service.h"
#include "components/signin/public/base/multilogin_parameters.h"
#include "components/signin/public/identity_manager/set_accounts_in_cookie_result.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace signin {

AccountsCookieMutatorImpl::MultiloginHelperWrapper::MultiloginHelperWrapper(
    std::unique_ptr<OAuthMultiloginHelper> helper)
    : helper_(std::move(helper)) {}

AccountsCookieMutatorImpl::MultiloginHelperWrapper::~MultiloginHelperWrapper() =
    default;

AccountsCookieMutatorImpl::AccountsCookieMutatorImpl(
    SigninClient* signin_client,
    ProfileOAuth2TokenService* token_service,
    GaiaCookieManagerService* gaia_cookie_manager_service,
    AccountTrackerService* account_tracker_service)
    : signin_client_(signin_client),
      token_service_(token_service),
      gaia_cookie_manager_service_(gaia_cookie_manager_service),
      account_tracker_service_(account_tracker_service) {
  DCHECK(signin_client_);
  DCHECK(token_service_);
  DCHECK(gaia_cookie_manager_service_);
  DCHECK(account_tracker_service_);
}

AccountsCookieMutatorImpl::~AccountsCookieMutatorImpl() = default;

void AccountsCookieMutatorImpl::AddAccountToCookie(
    const CoreAccountId& account_id,
    gaia::GaiaSource source,
    AddAccountToCookieCompletedCallback completion_callback) {
  gaia_cookie_manager_service_->AddAccountToCookie(
      account_id, source, std::move(completion_callback));
}

void AccountsCookieMutatorImpl::AddAccountToCookieWithToken(
    const CoreAccountId& account_id,
    const std::string& access_token,
    gaia::GaiaSource source,
    AddAccountToCookieCompletedCallback completion_callback) {
  gaia_cookie_manager_service_->AddAccountToCookieWithToken(
      account_id, access_token, source, std::move(completion_callback));
}

void AccountsCookieMutatorImpl::SetAccountsInCookie(
    const MultiloginParameters& parameters,
    gaia::GaiaSource source,
    base::OnceCallback<void(SetAccountsInCookieResult)>
        set_accounts_in_cookies_completed_callback) {
  std::vector<GaiaCookieManagerService::AccountIdGaiaIdPair> accounts;
  for (const auto& account_id : parameters.accounts_to_send) {
    accounts.push_back(make_pair(
        account_id, account_tracker_service_->GetAccountInfo(account_id).gaia));
  }
  gaia_cookie_manager_service_->SetAccountsInCookie(
      parameters.mode, accounts, source,
      std::move(set_accounts_in_cookies_completed_callback));
}

std::unique_ptr<AccountsCookieMutator::SetAccountsInCookieTask>
AccountsCookieMutatorImpl::SetAccountsInCookieForPartition(
    PartitionDelegate* partition_delegate,
    const MultiloginParameters& parameters,
    gaia::GaiaSource source,
    base::OnceCallback<void(SetAccountsInCookieResult)>
        set_accounts_in_cookies_completed_callback) {
  // The default partition must go through the GaiaCookieManagerService.
  DCHECK_NE(signin_client_->GetCookieManager(),
            partition_delegate->GetCookieManagerForPartition())
      << "The default partition is passed to "
      << "SetAccountsInCookieForPartition(). Use SetAccountsInCookie() "
      << "instead.";

  std::vector<GaiaCookieManagerService::AccountIdGaiaIdPair> accounts;
  for (const auto& account_id : parameters.accounts_to_send) {
    accounts.push_back(make_pair(
        account_id, account_tracker_service_->GetAccountInfo(account_id).gaia));
  }

  return std::make_unique<MultiloginHelperWrapper>(
      std::make_unique<OAuthMultiloginHelper>(
          signin_client_, partition_delegate, token_service_, parameters.mode,
          accounts, /*external_cc_result=*/std::string(), source,
          std::move(set_accounts_in_cookies_completed_callback)));
}

void AccountsCookieMutatorImpl::TriggerCookieJarUpdate() {
  gaia_cookie_manager_service_->TriggerListAccounts();
}

#if BUILDFLAG(IS_IOS)
void AccountsCookieMutatorImpl::ForceTriggerOnCookieChange() {
  gaia_cookie_manager_service_->ForceOnCookieChangeProcessing();
}
#endif

void AccountsCookieMutatorImpl::LogOutAllAccounts(
    gaia::GaiaSource source,
    LogOutFromCookieCompletedCallback completion_callback) {
  gaia_cookie_manager_service_->LogOutAllAccounts(
      source, std::move(completion_callback));
}

void AccountsCookieMutatorImpl::RemoveLoggedOutAccountByGaiaId(
    const std::string& gaia_id) {
  // Note that RemoveLoggedOutAccountByGaiaId() does NOT internally trigger a
  // ListAccounts fetch. It could make sense to force a request here, e.g. via
  // ForceOnCookieChangeProcessing(), but this isn't considered important enough
  // to justify the risk for overloading the server.
  gaia_cookie_manager_service_->RemoveLoggedOutAccountByGaiaId(gaia_id);
}

}  // namespace signin
