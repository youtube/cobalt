// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_TOKEN_HANDLE_STORE_H_
#define ASH_PUBLIC_CPP_TOKEN_HANDLE_STORE_H_

#include "base/functional/callback_forward.h"
#include "components/account_id/account_id.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ash {

// Common interface for classes responsible for:
// - orchestrating checks and fetches for token handles stored against the
// user’s account.
// - low-level interactions with the various token handle prefs.
// Token Handle is an access token associated with OAuth refresh token that has
// exactly the same lifetime. It is not secure, and its only purpose is checking
// validity of corresponding refresh token in an insecure environment, eg. login
// screen.
class TokenHandleStore {
 public:
  // `account_id`: The account for which the token handle check was performed.
  // `token`: The token which was checked. Empty if we could not find a token
  // handle for `account_id`.
  // `reauth_required`: Result of `IsReauthRequired()`. `true` means that
  // reauthentication is required for `account_id`.
  using TokenValidationCallback =
      base::OnceCallback<void(const AccountId& account_id,
                              const std::string& token,
                              bool reauth_required)>;

  virtual ~TokenHandleStore() = default;

  // Returns true if UserManager has token handle associated with `account_id`.
  virtual bool HasToken(const AccountId& account_id) const = 0;

  // Returns true if the token status for `account_id` was checked recently.
  virtual bool IsRecentlyChecked(const AccountId& account_id) const = 0;

  // Indicates if token handle for `account_id` is missing or marked as invalid.
  virtual bool ShouldObtainHandle(const AccountId& account_id) const = 0;

  // Performs token handle check for `account_id`. Will call `callback` with
  // corresponding result. See `TokenValidationCallback` for details.
  virtual void IsReauthRequired(
      const AccountId& account_id,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      TokenValidationCallback callback) = 0;

  // Given the token `handle` store it for `account_id`.
  virtual void StoreTokenHandle(const AccountId& account_id,
                                const std::string& handle) = 0;

  // Testing methods:
  virtual void SetInvalidTokenForTesting(const char* token) = 0;

  virtual void SetLastCheckedPrefForTesting(const AccountId& account_id,
                                            base::Time time) = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_TOKEN_HANDLE_STORE_H_
