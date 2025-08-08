// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/profile_identity_provider.h"

#include "base/functional/bind.h"
#include "components/signin/public/identity_manager/access_token_info.h"

namespace invalidation {

namespace {

// ActiveAccountAccessTokenFetcher implementation that is backed by
// IdentityManager and wraps an AccessTokenFetcher internally.
class AccessTokenFetcherAdaptor : public ActiveAccountAccessTokenFetcher {
 public:
  AccessTokenFetcherAdaptor(const CoreAccountId& active_account_id,
                            const std::string& oauth_consumer_name,
                            signin::IdentityManager* identity_manager,
                            const signin::ScopeSet& scopes,
                            ActiveAccountAccessTokenCallback callback);
  AccessTokenFetcherAdaptor(const AccessTokenFetcherAdaptor& other) = delete;
  AccessTokenFetcherAdaptor& operator=(const AccessTokenFetcherAdaptor& other) =
      delete;
  ~AccessTokenFetcherAdaptor() override = default;

 private:
  // Invokes |callback_| with (|error|, |access_token_info.token|).
  void HandleTokenRequestCompletion(GoogleServiceAuthError error,
                                    signin::AccessTokenInfo access_token_info);

  ActiveAccountAccessTokenCallback callback_;
  std::unique_ptr<signin::AccessTokenFetcher> access_token_fetcher_;
};

AccessTokenFetcherAdaptor::AccessTokenFetcherAdaptor(
    const CoreAccountId& active_account_id,
    const std::string& oauth_consumer_name,
    signin::IdentityManager* identity_manager,
    const signin::ScopeSet& scopes,
    ActiveAccountAccessTokenCallback callback)
    : callback_(std::move(callback)) {
  access_token_fetcher_ = identity_manager->CreateAccessTokenFetcherForAccount(
      active_account_id, oauth_consumer_name, scopes,
      base::BindOnce(&AccessTokenFetcherAdaptor::HandleTokenRequestCompletion,
                     base::Unretained(this)),
      signin::AccessTokenFetcher::Mode::kImmediate);
}

void AccessTokenFetcherAdaptor::HandleTokenRequestCompletion(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  access_token_fetcher_.reset();

  std::move(callback_).Run(error, access_token_info.token);
}

}  // namespace

ProfileIdentityProvider::ProfileIdentityProvider(
    signin::IdentityManager* identity_manager)
    : identity_manager_(identity_manager) {
  identity_manager_->AddObserver(this);

  active_account_id_ =
      identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
}

ProfileIdentityProvider::~ProfileIdentityProvider() {
  identity_manager_->RemoveObserver(this);
}

CoreAccountId ProfileIdentityProvider::GetActiveAccountId() {
  return active_account_id_;
}

bool ProfileIdentityProvider::IsActiveAccountWithRefreshToken() {
  if (GetActiveAccountId().empty() || !identity_manager_ ||
      !identity_manager_->HasAccountWithRefreshToken(GetActiveAccountId())) {
    return false;
  }

  return true;
}

std::unique_ptr<ActiveAccountAccessTokenFetcher>
ProfileIdentityProvider::FetchAccessToken(
    const std::string& oauth_consumer_name,
    const signin::ScopeSet& scopes,
    ActiveAccountAccessTokenCallback callback) {
  return std::make_unique<AccessTokenFetcherAdaptor>(
      GetActiveAccountId(), oauth_consumer_name, identity_manager_, scopes,
      std::move(callback));
}

void ProfileIdentityProvider::InvalidateAccessToken(
    const signin::ScopeSet& scopes,
    const std::string& access_token) {
  identity_manager_->RemoveAccessTokenFromCache(GetActiveAccountId(), scopes,
                                                access_token);
}

void ProfileIdentityProvider::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  CoreAccountId account_id =
      event_details.GetCurrentState().primary_account.account_id;

  if (account_id == active_account_id_) {
    return;
  }

  if (!active_account_id_.empty()) {
    FireOnActiveAccountLogout();
  }

  active_account_id_ = account_id;
  if (!active_account_id_.empty()) {
    FireOnActiveAccountLogin();
  }
}

void ProfileIdentityProvider::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  ProcessRefreshTokenUpdateForAccount(account_info.account_id);
}

void ProfileIdentityProvider::OnRefreshTokenRemovedForAccount(
    const CoreAccountId& account_id) {
  ProcessRefreshTokenRemovalForAccount(account_id);
}

}  // namespace invalidation
