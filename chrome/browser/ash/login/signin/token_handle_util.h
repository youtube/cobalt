// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SIGNIN_TOKEN_HANDLE_UTIL_H_
#define CHROME_BROWSER_ASH_LOGIN_SIGNIN_TOKEN_HANDLE_UTIL_H_

#include <string>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/account_id/account_id.h"
#include "google_apis/gaia/gaia_oauth_client.h"

class AccountId;

namespace ash {

// This class is responsible for operations with External Token Handle.
// Handle is an extra token associated with OAuth refresh token that have
// exactly same lifetime. It is not secure, and it's only purpose is checking
// validity of corresponding refresh token in the insecure environment.
class TokenHandleUtil {
 public:
  TokenHandleUtil();

  TokenHandleUtil(const TokenHandleUtil&) = delete;
  TokenHandleUtil& operator=(const TokenHandleUtil&) = delete;

  ~TokenHandleUtil();

  enum TokenHandleStatus { VALID, INVALID, UNKNOWN };

  using TokenValidationCallback =
      base::OnceCallback<void(const AccountId&, TokenHandleStatus)>;

  // Returns true if UserManager has token handle associated with `account_id`.
  static bool HasToken(const AccountId& account_id);

  // Returns true if the token status for `account_id` was checked recently
  // (within kCacheStatusTime).
  static bool IsRecentlyChecked(const AccountId& account_id);

  // Indicates if token handle for `account_id` is missing or marked as invalid.
  static bool ShouldObtainHandle(const AccountId& account_id);

  // Performs token handle check for `account_id`. Will call `callback` with
  // corresponding result.
  void CheckToken(
      const AccountId& account_id,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      TokenValidationCallback callback);

  // Given the token `handle` store it for `account_id`.
  static void StoreTokenHandle(const AccountId& account_id,
                               const std::string& handle);

  static void ClearTokenHandle(const AccountId& account_id);

  static void SetInvalidTokenForTesting(const char* token);

  static void SetLastCheckedPrefForTesting(const AccountId& account_id,
                                           base::Time time);

 private:
  // Associates GaiaOAuthClient::Delegate with User ID and Token.
  class TokenDelegate : public gaia::GaiaOAuthClient::Delegate {
   public:
    TokenDelegate(
        const base::WeakPtr<TokenHandleUtil>& owner,
        const AccountId& account_id,
        const std::string& token,
        scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
        TokenValidationCallback callback);

    TokenDelegate(const TokenDelegate&) = delete;
    TokenDelegate& operator=(const TokenDelegate&) = delete;

    ~TokenDelegate() override;

    void OnOAuthError() override;
    void OnNetworkError(int response_code) override;
    void OnGetTokenInfoResponse(const base::Value::Dict& token_info) override;
    void NotifyDone();

   private:
    base::WeakPtr<TokenHandleUtil> owner_;
    AccountId account_id_;
    std::string token_;
    base::TimeTicks tokeninfo_response_start_time_;
    TokenValidationCallback callback_;
    gaia::GaiaOAuthClient gaia_client_;
  };

  void OnValidationComplete(const std::string& token);

  // Map of pending check operations.
  base::flat_map<std::string, std::unique_ptr<TokenDelegate>>
      validation_delegates_;

  base::WeakPtrFactory<TokenHandleUtil> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SIGNIN_TOKEN_HANDLE_UTIL_H_
