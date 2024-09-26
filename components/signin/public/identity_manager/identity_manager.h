// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_IDENTITY_MANAGER_H_
#define COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_IDENTITY_MANAGER_H_

#include <memory>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/scoped_observation_traits.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/account_manager_core/account.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/internal/identity_manager/primary_account_manager.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service_observer.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_client.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_mutator.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "google_apis/gaia/oauth2_access_token_manager.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#include "base/containers/flat_map.h"
#include "base/time/time.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
namespace account_manager {
class AccountManagerFacade;
}
#endif

namespace gaia {
struct ListedAccount;
}  // namespace gaia

namespace network {
class SharedURLLoaderFactory;
class TestURLLoaderFactory;
}  // namespace network

class PrefRegistrySimple;

class AccountFetcherService;
class AccountTrackerService;
class GaiaCookieManagerService;
class NewTabPageUI;

namespace signin {

struct AccountsInCookieJarInfo;
class IdentityManagerTest;
class IdentityTestEnvironment;
class DiagnosticsProvider;
enum class ClearPrimaryAccountPolicy;
struct CookieParamsForTest;

// Gives access to information about the user's Google identities. See
// ./README.md for detailed documentation.
class IdentityManager : public KeyedService,
                        public OAuth2AccessTokenManager::DiagnosticsObserver,
                        public PrimaryAccountManager::Observer,
                        public ProfileOAuth2TokenServiceObserver {
 public:
  class Observer {
   public:
    Observer() = default;
    virtual ~Observer() = default;

    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;

    // Called when there is a change in the primary account or in the consent
    // level for the primary account.
    //
    // Note: Observers are not allowed to change the primary account directly
    // from this methood as that would lead to |event_details| not being correct
    // for the future observers.
    virtual void OnPrimaryAccountChanged(
        const PrimaryAccountChangeEvent& event_details) {}

    // Called when a new refresh token is associated with |account_info|.
    // NOTE: On a signin event, the ordering of this callback wrt the
    // OnPrimaryAccountSet() callback is undefined. If you as a client are
    // interested in both callbacks, PrimaryAccountAccessTokenFetcher will
    // likely meet your needs. Otherwise, if this lack of ordering is
    // problematic for your use case, please contact blundell@chromium.org.
    virtual void OnRefreshTokenUpdatedForAccount(
        const CoreAccountInfo& account_info) {}

    // Called when the refresh token previously associated with |account_id|
    // has been removed. At the time that this callback is invoked, there is
    // no longer guaranteed to be any AccountInfo associated with
    // |account_id|.
    // NOTE: It is not guaranteed that a call to
    // OnRefreshTokenUpdatedForAccount() has previously occurred for this
    // account due to corner cases.
    // TODO(https://crbug.com/884731): Eliminate these corner cases.
    // NOTE: On a signout event, the ordering of this callback wrt the
    // OnPrimaryAccountCleared() callback is undefined.If this lack of ordering
    // is problematic for your use case, please contact blundell@chromium.org.
    virtual void OnRefreshTokenRemovedForAccount(
        const CoreAccountId& account_id) {}

    // Called when the error state of the refresh token for |account_id| has
    // changed. Note: It is always called after
    // |OnRefreshTokenUpdatedForAccount| when the refresh token is updated. It
    // is not called when the refresh token is removed.
    virtual void OnErrorStateOfRefreshTokenUpdatedForAccount(
        const CoreAccountInfo& account_info,
        const GoogleServiceAuthError& error) {}

    // Called after refresh tokens are loaded.
    virtual void OnRefreshTokensLoaded() {}

    // Called whenever the list of Gaia accounts in the cookie jar has changed.
    // |accounts_in_cookie_jar_info.accounts| is ordered by the order of the
    // accounts in the cookie.
    //
    // This observer method is also called when fetching the list of accounts
    // in Gaia cookies fails after a number of internal retries. In this case:
    // * |error| hold the last error to fetch the list of accounts;
    // * |accounts_in_cookie_jar_info.accounts_are_fresh| is set to false as
    //   the accounts information is considered stale;
    // * |accounts_in_cookie_jar_info.accounts| holds the last list of known
    //   accounts in the cookie jar.
    virtual void OnAccountsInCookieUpdated(
        const AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
        const GoogleServiceAuthError& error) {}

    // Called when the Gaia cookie has been deleted explicitly by a user
    // action, e.g. from the settings or by an extension.
    virtual void OnAccountsCookieDeletedByUserAction() {}

    // Called after a batch of refresh token state chagnes is completed.
    virtual void OnEndBatchOfRefreshTokenStateChanges() {}

    // Called after an account is updated.
    virtual void OnExtendedAccountInfoUpdated(const AccountInfo& info) {}

    // Called after removing an account info.
    virtual void OnExtendedAccountInfoRemoved(const AccountInfo& info) {}

    // Called on Shutdown(), for observers that aren't KeyedServices to remove
    // their observers.
    virtual void OnIdentityManagerShutdown(IdentityManager* identity_manager) {}
  };

  // Methods to register or remove observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Provides access to the core information of the user's primary account.
  // The primary account may or may not be blessed with the sync consent.
  // Returns an empty struct if no such info is available, either because there
  // is no primary account yet or because the user signed out or the |consent|
  // level required |ConsentLevel::kSync| was not granted.
  // Returns a non-empty struct if the primary account exists and was granted
  // the required consent level.
  // TODO(1046746): Update (./README.md).
  CoreAccountInfo GetPrimaryAccountInfo(ConsentLevel consent_level) const;

  // Provides access to the account ID of the user's primary account. Simple
  // convenience wrapper over GetPrimaryAccountInfo().account_id.
  CoreAccountId GetPrimaryAccountId(ConsentLevel consent_level) const;

  // Returns whether the user's primary account is available. If consent is
  // |ConsentLevel::kSync| then true implies that the user has blessed this
  // account for sync.
  bool HasPrimaryAccount(ConsentLevel consent_level) const;

  // Creates an AccessTokenFetcher given the passed-in information.
  [[nodiscard]] std::unique_ptr<AccessTokenFetcher>
  CreateAccessTokenFetcherForAccount(const CoreAccountId& account_id,
                                     const std::string& oauth_consumer_name,
                                     const ScopeSet& scopes,
                                     AccessTokenFetcher::TokenCallback callback,
                                     AccessTokenFetcher::Mode mode);

  // Creates an AccessTokenFetcher given the passed-in information, allowing
  // to specify a custom |url_loader_factory| as well.
  [[nodiscard]] std::unique_ptr<AccessTokenFetcher>
  CreateAccessTokenFetcherForAccount(
      const CoreAccountId& account_id,
      const std::string& oauth_consumer_name,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const ScopeSet& scopes,
      AccessTokenFetcher::TokenCallback callback,
      AccessTokenFetcher::Mode mode);

  // If an entry exists in the cache of access tokens corresponding to the
  // given information, removes that entry; in this case, the next access token
  // request for |account_id| and |scopes| will fetch a new token from the
  // network. Otherwise, is a no-op.
  void RemoveAccessTokenFromCache(const CoreAccountId& account_id,
                                  const ScopeSet& scopes,
                                  const std::string& access_token);

  // Provides the information of all accounts that have refresh tokens.
  // NOTE: The accounts should not be assumed to be in any particular order; in
  // particular, they are not guaranteed to be in the order in which the
  // refresh tokens were added.
  std::vector<CoreAccountInfo> GetAccountsWithRefreshTokens() const;

  // Same functionality as GetAccountsWithRefreshTokens() but returning the
  // extended account information.
  std::vector<AccountInfo> GetExtendedAccountInfoForAccountsWithRefreshToken()
      const;

  // Returns true if (a) the primary account exists, and (b) a refresh token
  // exists for the primary account.
  bool HasPrimaryAccountWithRefreshToken(ConsentLevel consent_level) const;

  // Returns true if a refresh token exists for |account_id|.
  bool HasAccountWithRefreshToken(const CoreAccountId& account_id) const;

  // Returns true if all refresh tokens have been loaded from disk.
  bool AreRefreshTokensLoaded() const;

  // Returns true if (a) a refresh token exists for |account_id|, and (b) the
  // refresh token is in a persistent error state (defined as
  // GoogleServiceAuthError::IsPersistentError() returning true for the error
  // returned by GetErrorStateOfRefreshTokenForAccount(account_id)).
  bool HasAccountWithRefreshTokenInPersistentErrorState(
      const CoreAccountId& account_id) const;

  // Returns the error state of the refresh token associated with |account_id|.
  // In particular: Returns GoogleServiceAuthError::AuthErrorNone() if either
  // (a) no refresh token exists for |account_id|, or (b) the refresh token is
  // not in a persistent error state. Otherwise, returns the last persistent
  // error that was detected when using the refresh token.
  GoogleServiceAuthError GetErrorStateOfRefreshTokenForAccount(
      const CoreAccountId& account_id) const;

  // Returns extended information for account identified by |account_info|, or
  // an empty AccountInfo if the account is not found.
  // Note: these functions return an empty AccountInfo if no refresh token is
  // available for the account (in particular before tokens are loaded).
  AccountInfo FindExtendedAccountInfo(
      const CoreAccountInfo& account_info) const;
  // The same as `FindExtendedAccountInfo()` but finds an account by account ID.
  AccountInfo FindExtendedAccountInfoByAccountId(
      const CoreAccountId& account_id) const;
  // The same as `FindExtendedAccountInfo()` but finds an account by email.
  AccountInfo FindExtendedAccountInfoByEmailAddress(
      const std::string& email_address) const;
  // The same as `FindExtendedAccountInfo()` but finds an account by gaia ID.
  AccountInfo FindExtendedAccountInfoByGaiaId(const std::string& gaia_id) const;

  // Provides the information of all accounts that are present in the Gaia
  // cookie in the cookie jar, ordered by their order in the cookie.
  // If the returned accounts are not fresh, an internal update will be
  // triggered and there will be a subsequent invocation of
  // IdentityManager::Observer::OnAccountsInCookieJarChanged().
  AccountsInCookieJarInfo GetAccountsInCookieJar() const;

  // Returns pointer to the object used to change the signed-in state of the
  // primary account, if supported on the current platform. Otherwise, returns
  // null.
  PrimaryAccountMutator* GetPrimaryAccountMutator();

  // Returns pointer to the object used to seed accounts and mutate state of
  // accounts' refresh tokens, if supported on the current platform. Otherwise,
  // returns null.
  AccountsMutator* GetAccountsMutator();

  // Returns pointer to the object used to manipulate the cookies stored and the
  // accounts associated with them. Guaranteed to be non-null.
  AccountsCookieMutator* GetAccountsCookieMutator();

  // Returns pointer to the object used to seed accounts information from the
  // device-level accounts. May be null if the system has no such notion.
  DeviceAccountsSynchronizer* GetDeviceAccountsSynchronizer();

  // Observer interface for classes that want to monitor status of various
  // requests. Mostly useful in tests and debugging contexts (e.g., WebUI).
  class DiagnosticsObserver {
   public:
    DiagnosticsObserver() = default;
    virtual ~DiagnosticsObserver() = default;

    DiagnosticsObserver(const DiagnosticsObserver&) = delete;
    DiagnosticsObserver& operator=(const DiagnosticsObserver&) = delete;

    // Called when receiving request for access token.
    virtual void OnAccessTokenRequested(const CoreAccountId& account_id,
                                        const std::string& consumer_id,
                                        const ScopeSet& scopes) {}

    // Called when an access token request is completed. Contains diagnostic
    // information about the access token request.
    virtual void OnAccessTokenRequestCompleted(const CoreAccountId& account_id,
                                               const std::string& consumer_id,
                                               const ScopeSet& scopes,
                                               GoogleServiceAuthError error,
                                               base::Time expiration_time) {}

    // Called when an access token was removed.
    virtual void OnAccessTokenRemovedFromCache(const CoreAccountId& account_id,
                                               const ScopeSet& scopes) {}

    // Called when a new refresh token is available. Contains diagnostic
    // information about the source of the operation.
    virtual void OnRefreshTokenUpdatedForAccountFromSource(
        const CoreAccountId& account_id,
        bool is_refresh_token_valid,
        const std::string& source) {}

    // Called when a refresh token is removed. Contains diagnostic information
    // about the source that initiated the revokation operation.
    virtual void OnRefreshTokenRemovedForAccountFromSource(
        const CoreAccountId& account_id,
        const std::string& source) {}
  };

  void AddDiagnosticsObserver(DiagnosticsObserver* observer);
  void RemoveDiagnosticsObserver(DiagnosticsObserver* observer);

  //  **************************************************************************
  //  NOTE: All public methods and structures below are either intended to be
  //  used only by signin code, or are slated for deletion. Most IdentityManager
  //  consumers should not need to interact with any methods or structures below
  //  this line.
  //  **************************************************************************

  // The struct contains all fields required to initialize the
  // IdentityManager instance.
  struct InitParameters {
    std::unique_ptr<ProfileOAuth2TokenService> token_service;
    std::unique_ptr<AccountTrackerService> account_tracker_service;
    std::unique_ptr<AccountFetcherService> account_fetcher_service;
    std::unique_ptr<GaiaCookieManagerService> gaia_cookie_manager_service;
    std::unique_ptr<AccountsCookieMutator> accounts_cookie_mutator;
    std::unique_ptr<PrimaryAccountManager> primary_account_manager;
    std::unique_ptr<PrimaryAccountMutator> primary_account_mutator;
    std::unique_ptr<AccountsMutator> accounts_mutator;
    std::unique_ptr<DeviceAccountsSynchronizer> device_accounts_synchronizer;
    std::unique_ptr<DiagnosticsProvider> diagnostics_provider;
    AccountConsistencyMethod account_consistency =
        AccountConsistencyMethod::kDisabled;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    raw_ptr<SigninClient> signin_client = nullptr;
#endif
#if BUILDFLAG(IS_CHROMEOS)
    raw_ptr<account_manager::AccountManagerFacade> account_manager_facade =
        nullptr;
#endif

    InitParameters();
    InitParameters(InitParameters&&);
    ~InitParameters();

    InitParameters(const InitParameters&) = delete;
    InitParameters& operator=(const InitParameters&) = delete;
  };

  explicit IdentityManager(IdentityManager::InitParameters&& parameters);

  IdentityManager(const IdentityManager&) = delete;
  IdentityManager& operator=(const IdentityManager&) = delete;

  ~IdentityManager() override;

  // KeyedService:
  void Shutdown() override;

  // Performs initialization that is dependent on the network being
  // initialized.
  void OnNetworkInitialized();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Methods related to migration of account IDs from email to Gaia ID.
  // TODO(https://crbug.com/883272): Remove these once all platforms have
  // migrated to the new account_id based on gaia (currently, only ChromeOS
  // remains).

  // Possible values for the account ID migration state, needs to be kept in
  // sync with AccountTrackerService::AccountIdMigrationState.
  enum AccountIdMigrationState {
    MIGRATION_NOT_STARTED = 0,
    MIGRATION_IN_PROGRESS = 1,
    MIGRATION_DONE = 2,
    NUM_MIGRATION_STATES
  };

  // Returns the currently saved state of the migration of account IDs.
  AccountIdMigrationState GetAccountIdMigrationState() const;
#endif

  // Picks the correct account_id for the specified account depending on the
  // migration state.
  CoreAccountId PickAccountIdForAccount(const std::string& gaia,
                                        const std::string& email) const;

  // Methods used only by embedder-level factory classes.

  // Registers per-install prefs used by this class.
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);
  // Registers per-profile prefs used by this class.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Returns pointer to the object used to obtain diagnostics about the internal
  // state of IdentityManager.
  DiagnosticsProvider* GetDiagnosticsProvider();

  // Returns account consistency method for this profile.
  AccountConsistencyMethod GetAccountConsistency() {
    return account_consistency_;
  }

#if BUILDFLAG(IS_ANDROID)
  // Returns a pointer to the AccountTrackerService Java instance associated
  // with this object.
  // TODO(https://crbug.com/934688): Eliminate this method once
  // AccountTrackerService.java has no more client usage.
  base::android::ScopedJavaLocalRef<jobject>
  LegacyGetAccountTrackerServiceJavaObject();

  // Get the reference on the java IdentityManager.
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  // Provide the reference on the java IdentityMutator.
  base::android::ScopedJavaLocalRef<jobject> GetIdentityMutatorJavaObject();

  // This method refreshes the AccountInfo associated with |account_id|,
  // when the existing account info is stale, otherwise it doesn't fetch the
  // account info if it is valid.
  // This method triggers an OnExtendedAccountInfoUpdated()
  // callback if the info was successfully fetched.
  void RefreshAccountInfoIfStale(const CoreAccountId& account_id);

  // Overloads for calls from java:
  bool HasPrimaryAccount(JNIEnv* env) const;

  base::android::ScopedJavaLocalRef<jobject> GetPrimaryAccountInfo(
      JNIEnv* env,
      jint consent_level) const;

  base::android::ScopedJavaLocalRef<jobject> GetPrimaryAccountId(
      JNIEnv* env) const;

  base::android::ScopedJavaLocalRef<jobject>
  FindExtendedAccountInfoByEmailAddress(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& j_email) const;

  base::android::ScopedJavaLocalRef<jobjectArray> GetAccountsWithRefreshTokens(
      JNIEnv* env) const;

  // Refreshes account info with image for the given core account id.
  void RefreshAccountInfoIfStale(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_core_account_id);
#endif

 private:
  // These test helpers need to use some of the private methods below.
  friend CoreAccountInfo SetPrimaryAccount(IdentityManager* identity_manager,
                                           const std::string& email,
                                           ConsentLevel consent_level);
  friend void SetRefreshTokenForPrimaryAccount(
      IdentityManager* identity_manager,
      const std::string& token_value);
  friend void SetInvalidRefreshTokenForPrimaryAccount(
      IdentityManager* identity_manager);
  friend void RemoveRefreshTokenForPrimaryAccount(
      IdentityManager* identity_manager);
  friend AccountInfo MakePrimaryAccountAvailable(
      IdentityManager* identity_manager,
      const std::string& email,
      ConsentLevel consent_level);
  friend void RevokeSyncConsent(IdentityManager* identity_manager);
  friend void ClearPrimaryAccount(IdentityManager* identity_manager);
  friend AccountInfo MakeAccountAvailable(IdentityManager* identity_manager,
                                          const std::string& email);
  friend AccountInfo MakeAccountAvailableWithCookies(
      IdentityManager* identity_manager,
      network::TestURLLoaderFactory* test_url_loader_factory,
      const std::string& email,
      const std::string& gaia_id);
  friend void SetRefreshTokenForAccount(IdentityManager* identity_manager,
                                        const CoreAccountId& account_id,
                                        const std::string& token_value);
  friend void SetInvalidRefreshTokenForAccount(
      IdentityManager* identity_manager,
      const CoreAccountId& account_id);
  friend void RemoveRefreshTokenForAccount(IdentityManager* identity_manager,
                                           const CoreAccountId& account_id);
  friend void UpdateAccountInfoForAccount(IdentityManager* identity_manager,
                                          AccountInfo account_info);
  friend void SimulateAccountImageFetch(IdentityManager* identity_manager,
                                        const CoreAccountId& account_id,
                                        const std::string& image_url_with_size,
                                        const gfx::Image& image);
  friend void SetFreshnessOfAccountsInGaiaCookie(
      IdentityManager* identity_manager,
      bool accounts_are_fresh);
  friend void UpdatePersistentErrorOfRefreshTokenForAccount(
      IdentityManager* identity_manager,
      const CoreAccountId& account_id,
      const GoogleServiceAuthError& auth_error);

  friend void DisableAccessTokenFetchRetries(IdentityManager* identity_manager);
  friend void EnableAccountCapabilitiesFetches(
      IdentityManager* identity_manager);

  friend void CancelAllOngoingGaiaCookieOperations(
      IdentityManager* identity_manager);

  friend void SetCookieAccounts(
      IdentityManager* identity_manager,
      network::TestURLLoaderFactory* test_url_loader_factory,
      const std::vector<CookieParamsForTest>& cookie_accounts);

  friend void SimulateSuccessfulFetchOfAccountInfo(
      IdentityManager* identity_manager,
      const CoreAccountId& account_id,
      const std::string& email,
      const std::string& gaia,
      const std::string& hosted_domain,
      const std::string& full_name,
      const std::string& given_name,
      const std::string& locale,
      const std::string& picture_url);

#if BUILDFLAG(IS_CHROMEOS)
  friend account_manager::AccountManagerFacade* GetAccountManagerFacade(
      IdentityManager* identity_manager);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Temporary access to getters (e.g. GetTokenService()).
  // TODO(https://crbug.com/944127): Remove this friendship by
  // extending identity_test_utils.h as needed.
  friend IdentityTestEnvironment;

  // IdentityManagerTest reaches into IdentityManager internals in
  // order to drive its behavior.
  // TODO(https://crbug.com/943135): Find a better way to accomplish this.
  friend IdentityManagerTest;
  FRIEND_TEST_ALL_PREFIXES(IdentityManagerTest, Construct);
  FRIEND_TEST_ALL_PREFIXES(IdentityManagerTest,
                           PrimaryAccountInfoAfterSigninAndAccountRemoval);
  FRIEND_TEST_ALL_PREFIXES(IdentityManagerTest,
                           PrimaryAccountInfoAfterSigninAndRefreshTokenRemoval);
  FRIEND_TEST_ALL_PREFIXES(IdentityManagerTest, RemoveAccessTokenFromCache);
  FRIEND_TEST_ALL_PREFIXES(IdentityManagerTest,
                           CreateAccessTokenFetcherWithCustomURLLoaderFactory);
  FRIEND_TEST_ALL_PREFIXES(IdentityManagerTest, ObserveAccessTokenFetch);
  FRIEND_TEST_ALL_PREFIXES(IdentityManagerTest,
                           ObserveAccessTokenRequestCompletionWithRefreshToken);
  FRIEND_TEST_ALL_PREFIXES(IdentityManagerTest,
                           BatchChangeObserversAreNotifiedOnCredentialsUpdate);
  FRIEND_TEST_ALL_PREFIXES(IdentityManagerTest, RemoveAccessTokenFromCache);
  FRIEND_TEST_ALL_PREFIXES(IdentityManagerTest,
                           CreateAccessTokenFetcherWithCustomURLLoaderFactory);
  FRIEND_TEST_ALL_PREFIXES(
      IdentityManagerTest,
      CallbackSentOnUpdateToAccountsInCookieWithNoAccounts);
  FRIEND_TEST_ALL_PREFIXES(
      IdentityManagerTest,
      CallbackSentOnUpdateToAccountsInCookieWithOneAccount);
  FRIEND_TEST_ALL_PREFIXES(
      IdentityManagerTest,
      CallbackSentOnUpdateToAccountsInCookieWithTwoAccounts);
  FRIEND_TEST_ALL_PREFIXES(IdentityManagerTest,
                           CallbackSentOnUpdateToSignOutAccountsInCookie);
  FRIEND_TEST_ALL_PREFIXES(
      IdentityManagerTest,
      CallbackSentOnUpdateToAccountsInCookieWithStaleAccounts);
  FRIEND_TEST_ALL_PREFIXES(IdentityManagerTest,
                           CallbackSentOnSuccessfulAdditionOfAccountToCookie);
  FRIEND_TEST_ALL_PREFIXES(IdentityManagerTest,
                           CallbackSentOnFailureAdditionOfAccountToCookie);
  FRIEND_TEST_ALL_PREFIXES(IdentityManagerTest,
                           CallbackSentOnSetAccountsInCookieCompleted_Success);
  FRIEND_TEST_ALL_PREFIXES(IdentityManagerTest,
                           CallbackSentOnSetAccountsInCookieCompleted_Failure);
  FRIEND_TEST_ALL_PREFIXES(IdentityManagerTest,
                           CallbackSentOnAccountsCookieDeletedByUserAction);
  FRIEND_TEST_ALL_PREFIXES(IdentityManagerTest, OnNetworkInitialized);
  FRIEND_TEST_ALL_PREFIXES(IdentityManagerTest, RefreshAccountInfoIfStale);
  FRIEND_TEST_ALL_PREFIXES(IdentityManagerTest, FindExtendedPrimaryAccountInfo);

  // Only caller to FindExtendedPrimaryAccountInfo().
  // TODO(https://crbug.com/1213351): Delete once the private call has been
  // removed.
  friend class ::NewTabPageUI;

  // Returns the extended account info for the primary account. This function
  // does not require tokens to be loaded.
  // Do not add more external callers, as account info is generally not
  // available until tokens are loaded.
  // TODO(https://crbug.com/1213351): Remove existing external callers.
  AccountInfo FindExtendedPrimaryAccountInfo(ConsentLevel consent_level);

  // Private getters used for testing only (i.e. see identity_test_utils.h).
  PrimaryAccountManager* GetPrimaryAccountManager() const;
  ProfileOAuth2TokenService* GetTokenService() const;
  AccountTrackerService* GetAccountTrackerService() const;
  AccountFetcherService* GetAccountFetcherService() const;
  GaiaCookieManagerService* GetGaiaCookieManagerService() const;
#if BUILDFLAG(IS_CHROMEOS)
  account_manager::AccountManagerFacade* GetAccountManagerFacade() const;
#endif

  // Populates and returns an AccountInfo object corresponding to |account_id|,
  // which must be an account with a refresh token.
  AccountInfo GetAccountInfoForAccountWithRefreshToken(
      const CoreAccountId& account_id) const;

  // PrimaryAccountManager::Observer:
  void OnPrimaryAccountChanged(
      const PrimaryAccountChangeEvent& event_details) override;

  // ProfileOAuth2TokenServiceObserver:
  void OnRefreshTokenAvailable(const CoreAccountId& account_id) override;
  void OnRefreshTokenRevoked(const CoreAccountId& account_id) override;
  void OnRefreshTokensLoaded() override;
  void OnEndBatchChanges() override;
  void OnAuthErrorChanged(const CoreAccountId& account_id,
                          const GoogleServiceAuthError& auth_error) override;

  // GaiaCookieManagerService callbacks:
  void OnGaiaAccountsInCookieUpdated(
      const std::vector<gaia::ListedAccount>& signed_in_accounts,
      const std::vector<gaia::ListedAccount>& signed_out_accounts,
      const GoogleServiceAuthError& error);
  void OnGaiaCookieDeletedByUserAction();

  // OAuth2AccessTokenManager::DiagnosticsObserver
  void OnAccessTokenRequested(const CoreAccountId& account_id,
                              const std::string& consumer_id,
                              const ScopeSet& scopes) override;
  void OnFetchAccessTokenComplete(const CoreAccountId& account_id,
                                  const std::string& consumer_id,
                                  const ScopeSet& scopes,
                                  GoogleServiceAuthError error,
                                  base::Time expiration_time) override;
  void OnAccessTokenRemoved(const CoreAccountId& account_id,
                            const ScopeSet& scopes) override;

  // ProfileOAuth2TokenService callbacks:
  void OnRefreshTokenAvailableFromSource(const CoreAccountId& account_id,
                                         bool is_refresh_token_valid,
                                         const std::string& source);
  void OnRefreshTokenRevokedFromSource(const CoreAccountId& account_id,
                                       const std::string& source);

  // AccountTrackerService callbacks:
  void OnAccountUpdated(const AccountInfo& info);
  void OnAccountRemoved(const AccountInfo& info);

  // Backing signin classes.
  std::unique_ptr<AccountTrackerService> account_tracker_service_;
  std::unique_ptr<ProfileOAuth2TokenService> token_service_;
  std::unique_ptr<GaiaCookieManagerService> gaia_cookie_manager_service_;
  std::unique_ptr<PrimaryAccountManager> primary_account_manager_;
  std::unique_ptr<AccountFetcherService> account_fetcher_service_;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  const raw_ptr<SigninClient> signin_client_;
#endif
#if BUILDFLAG(IS_CHROMEOS)
  const raw_ptr<account_manager::AccountManagerFacade> account_manager_facade_;
#endif

  IdentityMutator identity_mutator_;

  // DiagnosticsProvider instance.
  std::unique_ptr<DiagnosticsProvider> diagnostics_provider_;

  // Scoped observers.
  base::ScopedObservation<PrimaryAccountManager,
                          PrimaryAccountManager::Observer>
      primary_account_manager_observation_{this};
  base::ScopedObservation<ProfileOAuth2TokenService,
                          ProfileOAuth2TokenServiceObserver>
      token_service_observation_{this};

  // Lists of observers.
  // Makes sure lists are empty on destruction.
  base::ObserverList<Observer, true>::Unchecked observer_list_;
  base::ObserverList<DiagnosticsObserver, true>::Unchecked
      diagnostics_observation_list_;

  AccountConsistencyMethod account_consistency_ =
      AccountConsistencyMethod::kDisabled;

#if BUILDFLAG(IS_ANDROID)
  // Java-side IdentityManager object.
  base::android::ScopedJavaGlobalRef<jobject> java_identity_manager_;

  // CoreAccountId and the corresponding fetch start time, this is only
  // used to record account information fetch duration.
  base::flat_map<CoreAccountId, base::TimeTicks>
      account_info_fetch_start_times_;
#endif
};

}  // namespace signin

namespace base {

template <>
struct ScopedObservationTraits<signin::IdentityManager,
                               signin::IdentityManager::DiagnosticsObserver> {
  static void AddObserver(
      signin::IdentityManager* source,
      signin::IdentityManager::DiagnosticsObserver* observer) {
    source->AddDiagnosticsObserver(observer);
  }
  static void RemoveObserver(
      signin::IdentityManager* source,
      signin::IdentityManager::DiagnosticsObserver* observer) {
    source->RemoveDiagnosticsObserver(observer);
  }
};

}  // namespace base

#endif  // COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_IDENTITY_MANAGER_H_
