// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/profile_oauth2_token_service_delegate.h"

#include "base/observer_list.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service_observer.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_consumer.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace {

const net::BackoffEntry::Policy kBackoffPolicy = {
    0 /* int num_errors_to_ignore */,

    1000 /* int initial_delay_ms */,

    2.0 /* double multiply_factor */,

    0.2 /* double jitter_factor */,

    15 * 60 * 1000 /* int64_t maximum_backoff_ms (15 minutes) */,

    -1 /* int64_t entry_lifetime_ms */,

    false /* bool always_use_initial_delay */,
};

}  // namespace

ProfileOAuth2TokenServiceDelegate::ScopedBatchChange::ScopedBatchChange(
    ProfileOAuth2TokenServiceDelegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);
  delegate_->StartBatchChanges();
}

ProfileOAuth2TokenServiceDelegate::ScopedBatchChange::~ScopedBatchChange() {
  delegate_->EndBatchChanges();
}

ProfileOAuth2TokenServiceDelegate::ProfileOAuth2TokenServiceDelegate(
    bool use_backoff)
    : batch_change_depth_(0) {
  if (use_backoff)
    backoff_entry_ = std::make_unique<net::BackoffEntry>(&kBackoffPolicy);
}

ProfileOAuth2TokenServiceDelegate::~ProfileOAuth2TokenServiceDelegate() =
    default;

bool ProfileOAuth2TokenServiceDelegate::ValidateAccountId(
    const CoreAccountId& account_id) const {
  bool valid = !account_id.empty();

  // If the account is given as an email, make sure its a canonical email.
  // Note that some tests don't use email strings as account id, and after
  // the gaia id migration it won't be an email.  So only check for
  // canonicalization if the account_id is suspected to be an email.
  if (account_id.ToString().find('@') != std::string::npos &&
      gaia::CanonicalizeEmail(account_id.ToString()) != account_id.ToString()) {
    valid = false;
  }

  DCHECK(valid);
  return valid;
}

void ProfileOAuth2TokenServiceDelegate::AddObserver(
    ProfileOAuth2TokenServiceObserver* observer) {
  observer_list_.AddObserver(observer);
}

void ProfileOAuth2TokenServiceDelegate::RemoveObserver(
    ProfileOAuth2TokenServiceObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

bool ProfileOAuth2TokenServiceDelegate::HasObserver() const {
  return !observer_list_.empty();
}

void ProfileOAuth2TokenServiceDelegate::StartBatchChanges() {
  ++batch_change_depth_;
}

void ProfileOAuth2TokenServiceDelegate::EndBatchChanges() {
  --batch_change_depth_;
  DCHECK_LE(0, batch_change_depth_);
  if (batch_change_depth_ == 0) {
    FireEndBatchChanges();
  }
}

void ProfileOAuth2TokenServiceDelegate::FireEndBatchChanges() {
  for (auto& observer : observer_list_)
    observer.OnEndBatchChanges();
}

void ProfileOAuth2TokenServiceDelegate::FireRefreshTokenAvailable(
    const CoreAccountId& account_id) {
  DCHECK(!account_id.empty());
  ScopedBatchChange batch(this);
  for (auto& observer : observer_list_)
    observer.OnRefreshTokenAvailable(account_id);
}

void ProfileOAuth2TokenServiceDelegate::FireRefreshTokenRevoked(
    const CoreAccountId& account_id) {
  DCHECK(!account_id.empty());
  ScopedBatchChange batch(this);
  for (auto& observer : observer_list_)
    observer.OnRefreshTokenRevoked(account_id);
}

void ProfileOAuth2TokenServiceDelegate::FireRefreshTokensLoaded() {
  for (auto& observer : observer_list_)
    observer.OnRefreshTokensLoaded();
}

void ProfileOAuth2TokenServiceDelegate::FireAuthErrorChanged(
    const CoreAccountId& account_id,
    const GoogleServiceAuthError& error) {
  DCHECK(!account_id.empty());
  for (auto& observer : observer_list_)
    observer.OnAuthErrorChanged(account_id, error);
}

std::string ProfileOAuth2TokenServiceDelegate::GetTokenForMultilogin(
    const CoreAccountId& account_id) const {
  return std::string();
}

scoped_refptr<network::SharedURLLoaderFactory>
ProfileOAuth2TokenServiceDelegate::GetURLLoaderFactory() const {
  return nullptr;
}

std::vector<CoreAccountId> ProfileOAuth2TokenServiceDelegate::GetAccounts()
    const {
  return std::vector<CoreAccountId>();
}

const net::BackoffEntry* ProfileOAuth2TokenServiceDelegate::BackoffEntry()
    const {
  return backoff_entry_.get();
}

void ProfileOAuth2TokenServiceDelegate::LoadCredentials(
    const CoreAccountId& primary_account_id,
    bool is_syncing) {
  NOTREACHED()
      << "ProfileOAuth2TokenServiceDelegate does not load credentials. "
         "Subclasses that need to load credentials must provide "
         "an implemenation of this method";
}

void ProfileOAuth2TokenServiceDelegate::ExtractCredentials(
    ProfileOAuth2TokenService* to_service,
    const CoreAccountId& account_id) {
  NOTREACHED();
}

bool ProfileOAuth2TokenServiceDelegate::FixRequestErrorIfPossible() {
  return false;
}

GoogleServiceAuthError ProfileOAuth2TokenServiceDelegate::GetAuthError(
    const CoreAccountId& account_id) const {
  auto it = errors_.find(account_id);
  return (it == errors_.end()) ? GoogleServiceAuthError::AuthErrorNone()
                               : it->second;
}

void ProfileOAuth2TokenServiceDelegate::UpdateAuthError(
    const CoreAccountId& account_id,
    const GoogleServiceAuthError& error,
    bool fire_auth_error_changed) {
  DVLOG(1) << "ProfileOAuth2TokenServiceDelegate::UpdateAuthError"
           << " account=" << account_id << " error=" << error.ToString();

  if (!RefreshTokenIsAvailable(account_id)) {
    DLOG(ERROR) << "Update auth error failed because account="
                << account_id.ToString() << "has no refresh token";
    DCHECK_EQ(GetAuthError(account_id),
              GoogleServiceAuthError::AuthErrorNone());
    return;
  }

  if (backoff_entry_) {
    backoff_entry_->InformOfRequest(!error.IsTransientError());
    if (error.IsTransientError())
      backoff_error_ = error;
  }
  ValidateAccountId(account_id);

  // Do not report connection errors as these are not actually auth errors.
  // We also want to avoid masking a "real" auth error just because we
  // subsequently get a transient network error.  We do keep it around though
  // to report for future requests being denied for "backoff" reasons.
  if (error.IsTransientError())
    return;

  // Scope errors are only relevant to the scope set of the request and it does
  // not imply that the account is in an error state.
  if (error.IsScopePersistentError())
    return;

  auto it = errors_.find(account_id);
  if (error.state() == GoogleServiceAuthError::NONE) {
    if (it == errors_.end())
      return;
    errors_.erase(it);
  } else {
    if (it != errors_.end() && it->second == error)
      return;
    errors_[account_id] = error;
  }

  if (fire_auth_error_changed)
    FireAuthErrorChanged(account_id, error);
}

void ProfileOAuth2TokenServiceDelegate::ClearAuthError(
    const absl::optional<CoreAccountId>& account_id) {
  if (!account_id.has_value()) {
    errors_.clear();
    return;
  }

  auto it = errors_.find(account_id.value());
  if (it != errors_.end())
    errors_.erase(it);
}

GoogleServiceAuthError ProfileOAuth2TokenServiceDelegate::BackOffError() const {
  return backoff_error_;
}

void ProfileOAuth2TokenServiceDelegate::ResetBackOffEntry() {
  if (!backoff_entry_) {
    NOTREACHED() << "Should be called only if `use_backoff` was true in the "
                    "constructor.";
    return;
  }
  backoff_entry_->Reset();
}
