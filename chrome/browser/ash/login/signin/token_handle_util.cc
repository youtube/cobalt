// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/signin/token_handle_util.h"

#include "base/json/values_util.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "components/user_manager/known_user.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ash {
namespace {

const char kTokenHandlePref[] = "PasswordTokenHandle";
const char kTokenHandleStatusPref[] = "TokenHandleStatus";
const char kTokenHandleLastCheckedPref[] = "TokenHandleLastChecked";
const char kTokenHandleRotated[] = "TokenHandleRotated";

const char kHandleStatusValid[] = "valid";
const char kHandleStatusInvalid[] = "invalid";

constexpr int kMaxRetries = 3;

constexpr base::TimeDelta kCacheStatusTime = base::Hours(1);

const char* g_invalid_token_for_testing = nullptr;

bool MaybeReturnCachedStatus(
    const AccountId& account_id,
    TokenHandleUtil::TokenValidationCallback* callback) {
  user_manager::KnownUser known_user(g_browser_process->local_state());
  const std::string* saved_status =
      known_user.FindStringPath(account_id, kTokenHandleStatusPref);
  if (!saved_status)
    return false;

  if (*saved_status == kHandleStatusValid) {
    std::move(*callback).Run(account_id, TokenHandleUtil::VALID);
    return true;
  }

  if (*saved_status == kHandleStatusInvalid) {
    std::move(*callback).Run(account_id, TokenHandleUtil::INVALID);
    return true;
  }

  NOTREACHED();
  return false;
}

void OnStatusChecked(TokenHandleUtil::TokenValidationCallback callback,
                     const std::string& token,
                     const AccountId& account_id,
                     TokenHandleUtil::TokenHandleStatus status) {
  user_manager::KnownUser known_user(g_browser_process->local_state());
  // Check that the token that was checked matches the latest known token.
  // (This may happen if token check took too long, and user went through
  // online sign-in and obtained new token during that time.
  if (const std::string* latest_token =
          known_user.FindStringPath(account_id, kTokenHandlePref)) {
    if (token != *latest_token) {
      LOG(WARNING) << "Outdated token, assuming status is unknown";
      std::move(callback).Run(account_id, TokenHandleUtil::UNKNOWN);
      return;
    }
  }

  if (status != TokenHandleUtil::UNKNOWN) {
    // Update last checked timestamp.
    known_user.SetPath(account_id, kTokenHandleLastCheckedPref,
                       base::TimeToValue(base::Time::Now()));
  }

  if (status == TokenHandleUtil::INVALID) {
    known_user.SetStringPref(account_id, kTokenHandleStatusPref,
                             kHandleStatusInvalid);
  }
  std::move(callback).Run(account_id, status);
}

// Checks if token handle is explicitly marked as INVALID for |account_id|.
bool HasTokenStatusInvalid(const AccountId& account_id) {
  user_manager::KnownUser known_user(g_browser_process->local_state());
  const std::string* status =
      known_user.FindStringPath(account_id, kTokenHandleStatusPref);

  return status && *status == kHandleStatusInvalid;
}

}  // namespace

TokenHandleUtil::TokenHandleUtil() = default;

TokenHandleUtil::~TokenHandleUtil() = default;

// static
bool TokenHandleUtil::HasToken(const AccountId& account_id) {
  user_manager::KnownUser known_user(g_browser_process->local_state());
  bool token_rotated =
      known_user.FindBoolPath(account_id, kTokenHandleRotated).value_or(false);
  if (!token_rotated && known_user.GetIsEnterpriseManaged(account_id)) {
    // Ignore not rotated token starting from M94 for enterprise users to avoid
    // blocking them on the login screen. Rotation started in M91.
    ClearTokenHandle(account_id);
    return false;
  }

  const std::string* token =
      known_user.FindStringPath(account_id, kTokenHandlePref);
  return token && !token->empty();
}

// static
bool TokenHandleUtil::IsRecentlyChecked(const AccountId& account_id) {
  user_manager::KnownUser known_user(g_browser_process->local_state());
  const base::Value* value =
      known_user.FindPath(account_id, kTokenHandleLastCheckedPref);
  if (!value)
    return false;

  absl::optional<base::Time> last_checked = base::ValueToTime(value);
  if (!last_checked.has_value()) {
    return false;
  }

  return base::Time::Now() - last_checked.value() < kCacheStatusTime;
}

// static
bool TokenHandleUtil::ShouldObtainHandle(const AccountId& account_id) {
  user_manager::KnownUser known_user(g_browser_process->local_state());
  bool token_rotated =
      known_user.FindBoolPath(account_id, kTokenHandleRotated).value_or(false);
  return !HasToken(account_id) || HasTokenStatusInvalid(account_id) ||
         !token_rotated;
}

// static
void TokenHandleUtil::CheckToken(
    const AccountId& account_id,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    TokenValidationCallback callback) {
  user_manager::KnownUser known_user(g_browser_process->local_state());
  const std::string* token =
      known_user.FindStringPath(account_id, kTokenHandlePref);
  if (!token) {
    std::move(callback).Run(account_id, UNKNOWN);
    return;
  }

  if (g_invalid_token_for_testing && g_invalid_token_for_testing == *token) {
    std::move(callback).Run(account_id, INVALID);
    return;
  }

  if (IsRecentlyChecked(account_id) &&
      MaybeReturnCachedStatus(account_id, &callback)) {
    return;
  }

  // If token is explicitly marked as invalid, it does not make sense to check
  // it again.
  if (HasTokenStatusInvalid(account_id)) {
    std::move(callback).Run(account_id, INVALID);
    return;
  }

  // Constructor starts validation.
  validation_delegates_[*token] = std::make_unique<TokenDelegate>(
      weak_factory_.GetWeakPtr(), account_id, *token,
      std::move(url_loader_factory),
      base::BindOnce(&OnStatusChecked, std::move(callback), *token));
}

// static
void TokenHandleUtil::StoreTokenHandle(const AccountId& account_id,
                                       const std::string& handle) {
  user_manager::KnownUser known_user(g_browser_process->local_state());

  known_user.SetStringPref(account_id, kTokenHandlePref, handle);
  known_user.SetStringPref(account_id, kTokenHandleStatusPref,
                           kHandleStatusValid);
  known_user.SetBooleanPref(account_id, kTokenHandleRotated, true);
  known_user.SetPath(account_id, kTokenHandleLastCheckedPref,
                     base::TimeToValue(base::Time::Now()));
}

// static
void TokenHandleUtil::ClearTokenHandle(const AccountId& account_id) {
  user_manager::KnownUser known_user(g_browser_process->local_state());

  known_user.RemovePref(account_id, kTokenHandlePref);
  known_user.RemovePref(account_id, kTokenHandleStatusPref);
  known_user.RemovePref(account_id, kTokenHandleRotated);
  known_user.RemovePref(account_id, kTokenHandleLastCheckedPref);
}

// static
void TokenHandleUtil::SetInvalidTokenForTesting(const char* token) {
  g_invalid_token_for_testing = token;
}

// static
void TokenHandleUtil::SetLastCheckedPrefForTesting(const AccountId& account_id,
                                                   base::Time time) {
  user_manager::KnownUser known_user(g_browser_process->local_state());
  known_user.SetPath(account_id, kTokenHandleLastCheckedPref,
                     base::TimeToValue(time));
}

void TokenHandleUtil::OnValidationComplete(const std::string& token) {
  validation_delegates_.erase(token);
}

TokenHandleUtil::TokenDelegate::TokenDelegate(
    const base::WeakPtr<TokenHandleUtil>& owner,
    const AccountId& account_id,
    const std::string& token,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    TokenValidationCallback callback)
    : owner_(owner),
      account_id_(account_id),
      token_(token),
      tokeninfo_response_start_time_(base::TimeTicks::Now()),
      callback_(std::move(callback)),
      gaia_client_(std::move(url_loader_factory)) {
  gaia_client_.GetTokenHandleInfo(token_, kMaxRetries, this);
}

TokenHandleUtil::TokenDelegate::~TokenDelegate() = default;

void TokenHandleUtil::TokenDelegate::OnOAuthError() {
  std::move(callback_).Run(account_id_, INVALID);
  NotifyDone();
}

// Warning: NotifyDone() deletes `this`
void TokenHandleUtil::TokenDelegate::NotifyDone() {
  if (owner_)
    owner_->OnValidationComplete(token_);
}

void TokenHandleUtil::TokenDelegate::OnNetworkError(int response_code) {
  std::move(callback_).Run(account_id_, UNKNOWN);
  NotifyDone();
}

void TokenHandleUtil::TokenDelegate::OnGetTokenInfoResponse(
    const base::Value::Dict& token_info) {
  TokenHandleStatus outcome = UNKNOWN;
  if (!token_info.Find("error")) {
    absl::optional<int> expires_in = token_info.FindInt("expires_in");
    if (expires_in)
      outcome = (*expires_in < 0) ? INVALID : VALID;
  }

  const base::TimeDelta duration =
      base::TimeTicks::Now() - tokeninfo_response_start_time_;
  UMA_HISTOGRAM_TIMES("Login.TokenCheckResponseTime", duration);
  std::move(callback_).Run(account_id_, outcome);
  NotifyDone();
}

}  // namespace ash
