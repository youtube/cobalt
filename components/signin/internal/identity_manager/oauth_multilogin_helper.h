// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_OAUTH_MULTILOGIN_HELPER_H_
#define COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_OAUTH_MULTILOGIN_HELPER_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/signin/internal/identity_manager/oauth_multilogin_token_fetcher.h"
#include "components/signin/public/base/hybrid_encryption_key.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/identity_manager/accounts_cookie_mutator.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_id.h"
#include "net/cookies/cookie_access_result.h"

class GaiaAuthFetcher;
class GoogleServiceAuthError;
class ProfileOAuth2TokenService;

namespace signin {

enum class SetAccountsInCookieResult;
class OAuthMultiloginTokenResponse;

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
class BoundSessionOAuthMultiLoginDelegate;
#endif

// This is a helper class that drives the OAuth multilogin process.
// The main steps are:
// - fetch access tokens with login scope,
// - call the oauth multilogin endpoint,
// - get the cookies from the response body, and set them in the cookie manager.
// It is safe to delete this object from within the callbacks.
class OAuthMultiloginHelper : public GaiaAuthConsumer {
 public:
  using AccountIdGaiaIdPair = std::pair<CoreAccountId, GaiaId>;

  OAuthMultiloginHelper(
      SigninClient* signin_client,
      AccountsCookieMutator::PartitionDelegate* partition_delegate,
      ProfileOAuth2TokenService* token_service,
      gaia::MultiloginMode mode,
      const std::vector<AccountIdGaiaIdPair>& accounts,
      const std::string& external_cc_result,
      const gaia::GaiaSource& gaia_source,
      base::OnceCallback<void(SetAccountsInCookieResult)> callback);

  OAuthMultiloginHelper(const OAuthMultiloginHelper&) = delete;
  OAuthMultiloginHelper& operator=(const OAuthMultiloginHelper&) = delete;

  ~OAuthMultiloginHelper() override;

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  void SetEphemeralKeyForTesting(HybridEncryptionKey ephemeral_key);
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

 private:
  // Starts fetching tokens with OAuthMultiloginTokenFetcher.
  void StartFetchingTokens();

  // Callbacks for OAuthMultiloginTokenFetcher.
  void OnMultiloginTokensSuccess(
      base::flat_map<CoreAccountId, OAuthMultiloginTokenResponse> tokens);
  void OnMultiloginTokensFailure(const GoogleServiceAuthError& error);

  // Actual call to the multilogin endpoint.
  void StartFetchingMultiLogin();

  // Overridden from GaiaAuthConsumer.
  void OnOAuthMultiloginFinished(const OAuthMultiloginResult& result) override;

  // Starts setting parsed cookies in browser.
  void StartSettingCookies(const OAuthMultiloginResult& result);

  // Callback for CookieManager::SetCanonicalCookie.
  void OnCookieSet(const std::string& cookie_name,
                   const std::string& cookie_domain,
                   net::CookieAccessResult access_result);

  raw_ptr<SigninClient> signin_client_;
  raw_ptr<AccountsCookieMutator::PartitionDelegate> partition_delegate_;
  raw_ptr<ProfileOAuth2TokenService> token_service_;

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  std::unique_ptr<BoundSessionOAuthMultiLoginDelegate> bound_session_delegate_;
#endif

  int fetcher_retries_ = 0;

  gaia::MultiloginMode mode_;
  // Account IDs to set in the cookie.
  const std::vector<AccountIdGaiaIdPair> accounts_;
  // See GaiaCookieManagerService::ExternalCcResultFetcher for details.
  const std::string external_cc_result_;
  // The Gaia source to be passed when creating GaiaAuthFetchers for the
  // OAuthmultilogin request.
  const gaia::GaiaSource gaia_source_;
  // OAuth tokens for each account ID in `accounts_`, or empty if is not
  // populated yet.
  base::flat_map<CoreAccountId, OAuthMultiloginTokenResponse> tokens_;
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  // Token binding challenges received from Gaia. Chrome should try to sign over
  // each challenge no more than once.
  base::flat_map<CoreAccountId, std::string> token_binding_challenges_;
  // Ephemeral key that could be used for cookie encryption. Reset at every
  // request retry.
  std::optional<HybridEncryptionKey> ephemeral_key_;
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

  base::OnceCallback<void(SetAccountsInCookieResult)> callback_;
  std::unique_ptr<GaiaAuthFetcher> gaia_auth_fetcher_;
  std::unique_ptr<OAuthMultiloginTokenFetcher> token_fetcher_;

  // List of pairs (cookie name and cookie domain) that have to be set in
  // cookie jar.
  std::set<std::pair<std::string, std::string>> cookies_to_set_;

  base::WeakPtrFactory<OAuthMultiloginHelper> weak_ptr_factory_{this};
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_OAUTH_MULTILOGIN_HELPER_H_
