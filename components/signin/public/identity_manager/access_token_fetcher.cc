// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/access_token_fetcher.h"

#include <utility>

#include "base/check_op.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "components/signin/public/identity_manager/access_token_constants.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace signin {

AccessTokenFetcher::AccessTokenFetcher(
    const CoreAccountId& account_id,
    const std::string& oauth_consumer_name,
    ProfileOAuth2TokenService* token_service,
    PrimaryAccountManager* primary_account_manager,
    const ScopeSet& scopes,
    TokenCallback callback,
    Mode mode)
    : AccessTokenFetcher(account_id,
                         oauth_consumer_name,
                         token_service,
                         primary_account_manager,
                         /*url_loader_factory=*/nullptr,
                         scopes,
                         std::move(callback),
                         mode) {}

AccessTokenFetcher::AccessTokenFetcher(
    const CoreAccountId& account_id,
    const std::string& oauth_consumer_name,
    ProfileOAuth2TokenService* token_service,
    PrimaryAccountManager* primary_account_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const ScopeSet& scopes,
    TokenCallback callback,
    Mode mode)
    : OAuth2AccessTokenManager::Consumer(oauth_consumer_name),
      account_id_(account_id),
      token_service_(token_service),
      primary_account_manager_(primary_account_manager),
      url_loader_factory_(std::move(url_loader_factory)),
      scopes_(scopes),
      mode_(mode),
      callback_(std::move(callback)) {
  if (mode_ == Mode::kImmediate || IsRefreshTokenAvailable()) {
    StartAccessTokenRequest();
    return;
  }

  // Start observing the IdentityManager. This observer will be removed either
  // when a refresh token is obtained and an access token request is started or
  // when this object is destroyed.
  token_service_observation_.Observe(token_service_.get());
}

AccessTokenFetcher::~AccessTokenFetcher() {}

void AccessTokenFetcher::VerifyScopeAccess() {
  if (account_id_.empty()) {
    // Fetching access tokens for an empty account should fail, but not crash.
    // Verifying the OAuth scopes based on the consent level is thus not needed.
    return;
  }

  // The consumer has privileged access to all scopes, return early.
  if (GetPrivilegedOAuth2Consumers().count(/*oauth_consumer_name=*/id())) {
    VLOG(1) << id() << " has access rights to scopes: "
            << base::JoinString(
                   std::vector<std::string>(scopes_.begin(), scopes_.end()),
                   ",");
    return;
  }

  for (const std::string& scope : scopes_) {
    CHECK(!GetPrivilegedOAuth2Scopes().count(scope)) << base::StringPrintf(
        "You are attempting to access a privileged scope '%s' without the "
        "required access, please file a bug for access at "
        "https://bugs.chromium.org/p/chromium/issues/"
        "list?q=component:Services>SignIn.",
        scope.c_str());
  }

  // Only validate scope access if the user has not given sync consent.
  if (!primary_account_manager_->HasPrimaryAccount(ConsentLevel::kSync)) {
    for (const std::string& scope : scopes_) {
      CHECK(GetUnconsentedOAuth2Scopes().count(scope)) << base::StringPrintf(
          "Consumer '%s' is requesting scope '%s' that requires user consent. "
          "Please check that the user has consented to Sync before "
          "using this API.",
          id().c_str(), scope.c_str());
    }
  }

  VLOG(1) << id() << " has access rights to scopes: "
          << base::JoinString(
                 std::vector<std::string>(scopes_.begin(), scopes_.end()), ",");
}

bool AccessTokenFetcher::IsRefreshTokenAvailable() const {
  DCHECK_EQ(Mode::kWaitUntilRefreshTokenAvailable, mode_);

  return token_service_->RefreshTokenIsAvailable(account_id_);
}

void AccessTokenFetcher::StartAccessTokenRequest() {
  DCHECK(mode_ == Mode::kImmediate || IsRefreshTokenAvailable());

  // By the time of starting an access token request, we should no longer be
  // listening for signin-related events.
  DCHECK(!token_service_observation_.IsObservingSource(token_service_.get()));

  // Note: We might get here even in cases where we know that there's no refresh
  // token. We're requesting an access token anyway, so that the token service
  // will generate an appropriate error code that we can return to the client.
  DCHECK(!access_token_request_);

  // Ensure that the client has the appropriate user consent for accessing the
  // OAuth API scopes in this request.
  VerifyScopeAccess();

  if (url_loader_factory_) {
    access_token_request_ = token_service_->StartRequestWithContext(
        account_id_, url_loader_factory_, scopes_, this);
    return;
  }

  access_token_request_ =
      token_service_->StartRequest(account_id_, scopes_, this);
}

void AccessTokenFetcher::OnRefreshTokenAvailable(
    const CoreAccountId& account_id) {
  DCHECK_EQ(Mode::kWaitUntilRefreshTokenAvailable, mode_);

  if (!IsRefreshTokenAvailable())
    return;

  DCHECK(token_service_observation_.IsObservingSource(token_service_.get()));
  token_service_observation_.Reset();

  StartAccessTokenRequest();
}

void AccessTokenFetcher::OnGetTokenSuccess(
    const OAuth2AccessTokenManager::Request* request,
    const OAuth2AccessTokenConsumer::TokenResponse& token_response) {
  DCHECK_EQ(request, access_token_request_.get());
  std::unique_ptr<OAuth2AccessTokenManager::Request> request_deleter(
      std::move(access_token_request_));

  RunCallbackAndMaybeDie(
      GoogleServiceAuthError::AuthErrorNone(),
      AccessTokenInfo(token_response.access_token,
                      token_response.expiration_time, token_response.id_token));

  // Potentially dead after the above invocation; nothing to do except return.
}

void AccessTokenFetcher::OnGetTokenFailure(
    const OAuth2AccessTokenManager::Request* request,
    const GoogleServiceAuthError& error) {
  DCHECK_EQ(request, access_token_request_.get());
  std::unique_ptr<OAuth2AccessTokenManager::Request> request_deleter(
      std::move(access_token_request_));

  RunCallbackAndMaybeDie(error, AccessTokenInfo());

  // Potentially dead after the above invocation; nothing to do except return.
}

void AccessTokenFetcher::RunCallbackAndMaybeDie(
    GoogleServiceAuthError error,
    AccessTokenInfo access_token_info) {
  // Per the contract of this class, it is allowed for consumers to delete this
  // object from within the callback that is run below. Hence, it is not safe to
  // add any code below this call.
  std::move(callback_).Run(std::move(error), std::move(access_token_info));
}

}  // namespace signin
