// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/profile_oauth2_token_service_delegate_ios.h"

#include <set>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/stl_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/values.h"
#include "components/signin/internal/identity_manager/account_tracker_service.h"
#include "components/signin/public/base/signin_client.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/ios/device_accounts_provider.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_id.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_fetcher.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace {

using AccessTokenInfo = DeviceAccountsProvider::AccessTokenInfo;
using AccessTokenResult = DeviceAccountsProvider::AccessTokenResult;
using TokenResponseBuilder = OAuth2AccessTokenConsumer::TokenResponse::Builder;

// Match the way Chromium handles authentication errors in
// google_apis/gaia/oauth2_access_token_fetcher.cc:
GoogleServiceAuthError GetGoogleServiceAuthErrorFromAuthenticationErrorCategory(
    AuthenticationErrorCategory error) {
  switch (error) {
    case kAuthenticationErrorCategoryUnknownErrors:
      // Treat all unknown error as unexpected service response errors.
      // This may be too general and may require a finer grain filtering.
      return GoogleServiceAuthError(
          GoogleServiceAuthError::UNEXPECTED_SERVICE_RESPONSE);
    case kAuthenticationErrorCategoryAuthorizationErrors:
      return GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_SERVER);
    case kAuthenticationErrorCategoryAuthorizationForbiddenErrors:
      // HTTP_FORBIDDEN (403) is treated as temporary error, because it may be
      // '403 Rate Limit Exceeded.' (for more details, see
      // google_apis/gaia/oauth2_access_token_fetcher.cc).
      return GoogleServiceAuthError(
          GoogleServiceAuthError::SERVICE_UNAVAILABLE);
    case kAuthenticationErrorCategoryNetworkServerErrors:
      // Just set the connection error state to FAILED.
      return GoogleServiceAuthError::FromConnectionError(net::ERR_FAILED);
    case kAuthenticationErrorCategoryUserCancellationErrors:
      return GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED);
    case kAuthenticationErrorCategoryUnknownIdentityErrors:
      return GoogleServiceAuthError(GoogleServiceAuthError::USER_NOT_SIGNED_UP);
  }
  NOTREACHED() << "unsupported error: " << static_cast<int>(error);
}

// Converts a DeviceAccountsProvider::AccountInfo to an AccountInfo.
AccountInfo AccountInfoFromDeviceAccount(
    const DeviceAccountsProvider::AccountInfo& account) {
  AccountInfo account_info;
  account_info.email = account.GetEmail();
  account_info.gaia = account.GetGaiaId();
  account_info.hosted_domain = account.GetHostedDomain();
  return account_info;
}

GoogleServiceAuthError GoogleServiceAuthErrorFromDeviceAccount(
    const DeviceAccountsProvider::AccountInfo& account) {
  return account.HasPersistentAuthError()
             ? GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
                   GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                       CREDENTIALS_REJECTED_BY_SERVER)
             : GoogleServiceAuthError::AuthErrorNone();
}

class SSOAccessTokenFetcher : public OAuth2AccessTokenFetcher {
 public:
  SSOAccessTokenFetcher(OAuth2AccessTokenConsumer* consumer,
                        DeviceAccountsProvider* provider,
                        const AccountInfo& account);

  SSOAccessTokenFetcher(const SSOAccessTokenFetcher&) = delete;
  SSOAccessTokenFetcher& operator=(const SSOAccessTokenFetcher&) = delete;

  ~SSOAccessTokenFetcher() override;

  void Start(const std::string& client_id,
             const std::string& client_secret,
             const std::vector<std::string>& scopes) override;

  void CancelRequest() override;

  // Handles an access token response.
  void OnAccessTokenResponse(AccessTokenResult result);

 private:
  DeviceAccountsProvider* provider_;  // weak
  AccountInfo account_;
  bool request_was_cancelled_;
  base::WeakPtrFactory<SSOAccessTokenFetcher> weak_factory_;
};

SSOAccessTokenFetcher::SSOAccessTokenFetcher(
    OAuth2AccessTokenConsumer* consumer,
    DeviceAccountsProvider* provider,
    const AccountInfo& account)
    : OAuth2AccessTokenFetcher(consumer),
      provider_(provider),
      account_(account),
      request_was_cancelled_(false),
      weak_factory_(this) {
  DCHECK(provider_);
}

SSOAccessTokenFetcher::~SSOAccessTokenFetcher() = default;

void SSOAccessTokenFetcher::Start(const std::string& client_id,
                                  const std::string& client_secret_unused,
                                  const std::vector<std::string>& scopes) {
  std::set<std::string> scopes_set(scopes.begin(), scopes.end());
  provider_->GetAccessToken(
      account_.gaia, client_id, scopes_set,
      base::BindOnce(&SSOAccessTokenFetcher::OnAccessTokenResponse,
                     weak_factory_.GetWeakPtr()));
}

void SSOAccessTokenFetcher::CancelRequest() {
  request_was_cancelled_ = true;
}

void SSOAccessTokenFetcher::OnAccessTokenResponse(AccessTokenResult result) {
  if (request_was_cancelled_) {
    // Ignore the callback if the request was cancelled.
    return;
  }

  if (result.has_value()) {
    const AccessTokenInfo& info = result.value();
    FireOnGetTokenSuccess(TokenResponseBuilder()
                              .WithAccessToken(info.token)
                              .WithExpirationTime(info.expiration_time)
                              .build());
  } else {
    FireOnGetTokenFailure(
        GetGoogleServiceAuthErrorFromAuthenticationErrorCategory(
            result.error()));
  }
}

}  // namespace

ProfileOAuth2TokenServiceIOSDelegate::ProfileOAuth2TokenServiceIOSDelegate(
    SigninClient* client,
    std::unique_ptr<DeviceAccountsProvider> provider,
    AccountTrackerService* account_tracker_service)
    : ProfileOAuth2TokenServiceDelegate(/*use_backoff=*/false),
      client_(client),
      provider_(std::move(provider)),
      account_tracker_service_(account_tracker_service) {
  DCHECK(client_);
  DCHECK(provider_);
  DCHECK(account_tracker_service_);
  device_accounts_provider_observation_.Observe(provider_.get());
}

ProfileOAuth2TokenServiceIOSDelegate::~ProfileOAuth2TokenServiceIOSDelegate() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void ProfileOAuth2TokenServiceIOSDelegate::Shutdown() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  accounts_.clear();
  ClearAuthError(std::nullopt);
}

void ProfileOAuth2TokenServiceIOSDelegate::LoadCredentialsInternal(
    const CoreAccountId& primary_account_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  DCHECK_EQ(signin::LoadCredentialsState::LOAD_CREDENTIALS_NOT_STARTED,
            load_credentials_state());
  set_load_credentials_state(
      signin::LoadCredentialsState::LOAD_CREDENTIALS_IN_PROGRESS);

  ReloadCredentials(primary_account_id);
  if (primary_account_id.empty() ||
      RefreshTokenIsAvailable(primary_account_id)) {
    set_load_credentials_state(
        signin::LoadCredentialsState::LOAD_CREDENTIALS_FINISHED_WITH_SUCCESS);
  } else {
    // Account must have been seeded before (when the primary account was set).
    DCHECK(!account_tracker_service_->GetAccountInfo(primary_account_id)
                .gaia.empty());
    DCHECK(!account_tracker_service_->GetAccountInfo(primary_account_id)
                .email.empty());

    // For whatever reason, we failed to load the device account for the primary
    // account. There must always be an account for the primary account
    accounts_.insert(primary_account_id);
    UpdateAuthError(primary_account_id,
                    GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
                        GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                            CREDENTIALS_MISSING));
    FireRefreshTokenAvailable(primary_account_id);
    set_load_credentials_state(
        signin::LoadCredentialsState::
            LOAD_CREDENTIALS_FINISHED_WITH_NO_TOKEN_FOR_PRIMARY_ACCOUNT);
  }
  FireRefreshTokensLoaded();
}

void ProfileOAuth2TokenServiceIOSDelegate::ReloadCredentials(
    const CoreAccountId& primary_account_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Get the list of new account ids.
  std::set<CoreAccountId> new_account_ids;
  base::flat_map<CoreAccountId, DeviceAccountsProvider::AccountInfo>
      new_accounts =
          base::MakeFlatMap<CoreAccountId, DeviceAccountsProvider::AccountInfo>(
              provider_->GetAccountsForProfile(), {},
              [](const DeviceAccountsProvider::AccountInfo& account) {
                return std::make_pair(
                    CoreAccountId::FromGaiaId(account.GetGaiaId()), account);
              });
  for (const auto& [new_account_id, new_account] : new_accounts) {
    DCHECK(!new_account.GetGaiaId().empty());
    DCHECK(!new_account.GetEmail().empty());

    // Account must to be seeded before adding an account to ensure that
    // the GAIA ID is available if any client of this token service starts
    // a fetch access token operation when it receives a
    // |OnRefreshTokenAvailable| notification.
    CoreAccountId account_id = account_tracker_service_->SeedAccountInfo(
        AccountInfoFromDeviceAccount(new_account));
    new_account_ids.insert(account_id);
  }

  // Get the list of existing account ids.
  std::vector<CoreAccountId> old_account_ids = GetAccounts();
  std::sort(old_account_ids.begin(), old_account_ids.end());

  std::set<CoreAccountId> accounts_to_add =
      base::STLSetDifference<std::set<CoreAccountId>>(new_account_ids,
                                                      old_account_ids);
  std::set<CoreAccountId> accounts_to_remove =
      base::STLSetDifference<std::set<CoreAccountId>>(old_account_ids,
                                                      new_account_ids);
  if (accounts_to_add.empty() && accounts_to_remove.empty()) {
    return;
  }

  // Remove all old accounts that do not appear in |new_accounts| and then
  // load |new_accounts|.
  ScopedBatchChange batch(this);
  for (const auto& account_to_remove : accounts_to_remove) {
    if (account_to_remove == primary_account_id) {
      UpdateAuthError(account_to_remove,
                      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
                          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                              CREDENTIALS_MISSING));
    } else {
      RemoveAccount(account_to_remove);
    }
  }

  // Load all new_accounts.
  for (const auto& account_to_add : accounts_to_add) {
    GoogleServiceAuthError error = GoogleServiceAuthError::AuthErrorNone();
    if (base::FeatureList::IsEnabled(switches::kEnableIdentityInAuthError)) {
      error = GoogleServiceAuthErrorFromDeviceAccount(
          new_accounts.at(account_to_add));
    }
    AddOrUpdateAccount(account_to_add, error);
  }
}

void ProfileOAuth2TokenServiceIOSDelegate::UpdateCredentialsInternal(
    const CoreAccountId& account_id,
    const std::string& refresh_token) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  NOTREACHED() << "Unexpected call to UpdateCredentials when using shared "
                  "authentication.";
}

void ProfileOAuth2TokenServiceIOSDelegate::RevokeAllCredentialsInternal(
    signin_metrics::SourceForRefreshTokenOperation source) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  ScopedBatchChange batch(this);
  std::set<CoreAccountId> toRemove = accounts_;
  for (auto& account_id : toRemove) {
    RemoveAccount(account_id);
  }

  DCHECK_EQ(0u, accounts_.size());
}

void ProfileOAuth2TokenServiceIOSDelegate::
    ReloadAllAccountsFromSystemWithPrimaryAccount(
        const std::optional<CoreAccountId>& primary_account_id) {
  ReloadCredentials(primary_account_id.value_or(CoreAccountId()));
}

void ProfileOAuth2TokenServiceIOSDelegate::ReloadAccountFromSystem(
    const CoreAccountId& account_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  GoogleServiceAuthError error = GoogleServiceAuthError::AuthErrorNone();
  if (base::FeatureList::IsEnabled(switches::kEnableIdentityInAuthError)) {
    for (const auto& account : provider_->GetAccountsForProfile()) {
      if (account_id != CoreAccountId::FromGaiaId(account.GetGaiaId())) {
        continue;
      }
      error = GoogleServiceAuthErrorFromDeviceAccount(account);
      break;
    }
  }

  AddOrUpdateAccount(account_id, error);
}

std::unique_ptr<OAuth2AccessTokenFetcher>
ProfileOAuth2TokenServiceIOSDelegate::CreateAccessTokenFetcher(
    const CoreAccountId& account_id,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    OAuth2AccessTokenConsumer* consumer,
    const std::string& token_binding_challenge) {
  AccountInfo account_info =
      account_tracker_service_->GetAccountInfo(account_id);
  return std::make_unique<SSOAccessTokenFetcher>(consumer, provider_.get(),
                                                 account_info);
}

#if BUILDFLAG(IS_IOS)
void ProfileOAuth2TokenServiceIOSDelegate::GetRefreshTokenFromDevice(
    const CoreAccountId& account_id,
    const OAuth2AccessTokenManager::ScopeSet& scopes,
    signin::AccessTokenFetcher::TokenCallback callback) {
  CHECK(RefreshTokenIsAvailableOnDevice(account_id));
  std::set<std::string> scopes_set(scopes.begin(), scopes.end());
  provider_->GetAccessToken(
      GaiaId(account_id.ToString()),
      GaiaUrls::GetInstance()->oauth2_chrome_client_id(), scopes_set,
      base::BindOnce(
          [](signin::AccessTokenFetcher::TokenCallback callback,
             AccessTokenResult result) {
            if (result.has_value()) {
              const AccessTokenInfo& info = result.value();
              std::move(callback).Run(
                  GoogleServiceAuthError::AuthErrorNone(),
                  signin::AccessTokenInfo(info.token, info.expiration_time,
                                          std::string()));
            } else {
              std::move(callback).Run(
                  GetGoogleServiceAuthErrorFromAuthenticationErrorCategory(
                      result.error()),
                  signin::AccessTokenInfo());
            }
          },
          std::move(callback)));
}
#endif

std::vector<CoreAccountId> ProfileOAuth2TokenServiceIOSDelegate::GetAccounts()
    const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return std::vector<CoreAccountId>(accounts_.begin(), accounts_.end());
}

std::vector<AccountInfo>
ProfileOAuth2TokenServiceIOSDelegate::GetAccountsOnDevice() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // TODO(crbug.com/368409110): Cache these accounts somewhere, maybe in a
  // separate AccountTrackerService instance.
  std::vector<AccountInfo> account_infos;
  for (const auto& account : provider_->GetAccountsOnDevice()) {
    CHECK(!account.GetGaiaId().empty());
    CHECK(!account.GetEmail().empty());
    AccountInfo account_info;
    account_info.account_id = CoreAccountId::FromGaiaId(account.GetGaiaId());
    account_info.gaia = account.GetGaiaId();
    account_info.email = account.GetEmail();
    account_info.hosted_domain = account.GetHostedDomain();
    // TODO(crbug.com/368409110): Find a way to determine the full AccountInfo
    // for these accounts, not only the "core" fields.
    account_infos.push_back(std::move(account_info));
  }
  return account_infos;
}

bool ProfileOAuth2TokenServiceIOSDelegate::RefreshTokenIsAvailable(
    const CoreAccountId& account_id) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  return accounts_.count(account_id) > 0;
}

bool ProfileOAuth2TokenServiceIOSDelegate::RefreshTokenIsAvailableOnDevice(
    const CoreAccountId& account_id) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  for (const auto& account : provider_->GetAccountsOnDevice()) {
    CHECK(!account.GetGaiaId().empty());
    CHECK(!account.GetEmail().empty());
    if (account.GetGaiaId().ToString() == account_id.ToString()) {
      return true;
    }
  }
  return false;
}

// Clear the authentication error state and notify all observers that a new
// refresh token is available so that they request new access tokens.
void ProfileOAuth2TokenServiceIOSDelegate::AddOrUpdateAccount(
    const CoreAccountId& account_id,
    GoogleServiceAuthError error) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Account must have been seeded before attempting to add it.
  DCHECK(!account_tracker_service_->GetAccountInfo(account_id).gaia.empty());
  DCHECK(!account_tracker_service_->GetAccountInfo(account_id).email.empty());

  bool account_present = accounts_.count(account_id) > 0;
  if (account_present && GetAuthError(account_id) == error) {
    // No need to update the account if it is already a known account and if
    // the error didn't change.
    return;
  }

  accounts_.insert(account_id);
  UpdateAuthError(account_id, error,
                  /*fire_auth_error_changed=*/false);
  FireAuthErrorChanged(account_id, error);
  FireRefreshTokenAvailable(account_id);
}

void ProfileOAuth2TokenServiceIOSDelegate::OnAccountsOnDeviceChanged() {
  FireAccountsOnDeviceChanged();
}

void ProfileOAuth2TokenServiceIOSDelegate::OnAccountOnDeviceUpdated(
    const DeviceAccountsProvider::AccountInfo& device_account) {
  // Note: Ideally, only notifications about accounts that are *not* in the
  // current profile would be forwarded here, since AccountTrackerService takes
  // care of notifying observers about accounts in the profile anyway. But
  // currently, AccountTrackerService doesn't know about some account
  // properties, specifically about the account avatar, and those notifications
  // would otherwise not get propagated at all.
  FireAccountOnDeviceUpdated(AccountInfoFromDeviceAccount(device_account));
}

void ProfileOAuth2TokenServiceIOSDelegate::RemoveAccount(
    const CoreAccountId& account_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!account_id.empty());

  if (accounts_.count(account_id) > 0) {
    accounts_.erase(account_id);
    ClearAuthError(account_id);
    FireRefreshTokenRevoked(account_id);
  }
}
