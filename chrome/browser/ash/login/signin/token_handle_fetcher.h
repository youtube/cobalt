// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SIGNIN_TOKEN_HANDLE_FETCHER_H_
#define CHROME_BROWSER_ASH_LOGIN_SIGNIN_TOKEN_HANDLE_FETCHER_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/account_id/account_id.h"
#include "components/keyed_service/core/keyed_service_shutdown_notifier.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "google_apis/gaia/gaia_oauth_client.h"

class Profile;

namespace signin {
class IdentityManager;
}

namespace ash {
class TokenHandleUtil;

// This class is resposible for obtaining new token handle for user.
// It can be used in two ways. When user have just used GAIA signin there is
// an OAuth2 token available. If there is profile already loaded, then
// minting additional access token might be required.
class TokenHandleFetcher : public gaia::GaiaOAuthClient::Delegate {
 public:
  TokenHandleFetcher(TokenHandleUtil* util, const AccountId& account_id);

  TokenHandleFetcher(const TokenHandleFetcher&) = delete;
  TokenHandleFetcher& operator=(const TokenHandleFetcher&) = delete;

  ~TokenHandleFetcher() override;

  using TokenFetchingCallback =
      base::OnceCallback<void(const AccountId&, bool success)>;

  // Get token handle for user who have just signed in via GAIA. This
  // request will be performed using signin profile.
  void FillForNewUser(const std::string& access_token,
                      TokenFetchingCallback callback);

  // Get token handle for existing user.
  void BackfillToken(Profile* profile, TokenFetchingCallback callback);

  static void EnsureFactoryBuilt();

 private:
  // AccessTokenFetcher::TokenCallback for PrimaryAccountAccessTokenFetcher.
  void OnAccessTokenFetchComplete(GoogleServiceAuthError error,
                                  signin::AccessTokenInfo token_info);

  // GaiaOAuthClient::Delegate overrides:
  void OnOAuthError() override;
  void OnNetworkError(int response_code) override;
  void OnGetTokenInfoResponse(const base::Value::Dict& token_info) override;

  void FillForAccessToken(const std::string& access_token);

  // This is called before profile is detroyed.
  void OnProfileDestroyed();

  raw_ptr<TokenHandleUtil, ExperimentalAsh> token_handle_util_ = nullptr;
  AccountId account_id_;
  raw_ptr<signin::IdentityManager, ExperimentalAsh> identity_manager_ = nullptr;

  raw_ptr<Profile, ExperimentalAsh> profile_ = nullptr;
  base::TimeTicks tokeninfo_response_start_time_ = base::TimeTicks();
  TokenFetchingCallback callback_;
  std::unique_ptr<gaia::GaiaOAuthClient> gaia_client_;
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      access_token_fetcher_;
  base::CallbackListSubscription profile_shutdown_subscription_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SIGNIN_TOKEN_HANDLE_FETCHER_H_
