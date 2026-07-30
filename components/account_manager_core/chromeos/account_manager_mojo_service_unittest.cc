// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/account_manager_core/chromeos/account_manager_mojo_service.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/crosapi/mojom/account_manager.mojom-test-utils.h"
#include "chromeos/crosapi/mojom/account_manager.mojom.h"
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/account_manager_metrics.h"
#include "components/account_manager_core/account_manager_util.h"
#include "components/account_manager_core/chromeos/account_manager.h"
#include "components/account_manager_core/chromeos/account_manager_ui.h"
#include "components/account_manager_core/chromeos/fake_account_manager_ui.h"
#include "components/prefs/testing_pref_service.h"
#include "google_apis/gaia/gaia_id.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_fetcher.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crosapi {

namespace {

const GaiaId::Literal kFakeGaiaId("fake-gaia-id");
const char kFakeEmail[] = "fake_email@example.com";
const char kFakeToken[] = "fake-token";
const char kFakeOAuthConsumerName[] = "fake-oauth-consumer-name";
constexpr char kFakeAccessToken[] = "fake-access-token";
// Same access token value as above in `kFakeAccessToken`.
constexpr char kAccessTokenResponse[] = R"(
    {
      "access_token": "fake-access-token",
      "expires_in": 3600,
      "token_type": "Bearer",
      "id_token": "id_token"
    })";
const account_manager::Account kFakeAccount = account_manager::Account{
    account_manager::AccountKey::FromGaiaId(kFakeGaiaId), kFakeEmail};

}  // namespace

class TestAccountManagerObserver
    : public mojom::AccountManagerObserverInterceptorForTesting {
 public:
  TestAccountManagerObserver() : receiver_(this) {}

  TestAccountManagerObserver(const TestAccountManagerObserver&) = delete;
  TestAccountManagerObserver& operator=(const TestAccountManagerObserver&) =
      delete;
  ~TestAccountManagerObserver() override = default;

  void Observe(
      mojom::AccountManagerAsyncWaiter* const account_manager_async_waiter) {
    mojo::PendingReceiver<mojom::AccountManagerObserver> receiver;
    account_manager_async_waiter->AddObserver(&receiver);
    receiver_.Bind(std::move(receiver));
  }

  int GetNumSigninDialogClosedNotifications() const {
    return num_signin_dialog_closed_notifications_;
  }

 private:
  // mojom::AccountManagerObserverInterceptorForTesting override:
  AccountManagerObserver* GetForwardingInterface() override { return this; }

  // mojom::AccountManagerObserverInterceptorForTesting override:
  void OnSigninDialogClosed() override {
    ++num_signin_dialog_closed_notifications_;
  }

  int num_signin_dialog_closed_notifications_ = 0;
  mojo::Receiver<mojom::AccountManagerObserver> receiver_;
};

// A test spy for intercepting AccountManager calls.
class AccountManagerSpy : public account_manager::AccountManager {
 public:
  AccountManagerSpy() = default;
  AccountManagerSpy(const AccountManagerSpy&) = delete;
  AccountManagerSpy& operator=(const AccountManagerSpy&) = delete;
  ~AccountManagerSpy() override = default;

  // account_manager::AccountManager override:
  std::unique_ptr<OAuth2AccessTokenFetcher> CreateAccessTokenFetcher(
      const ::account_manager::AccountKey& account_key,
      OAuth2AccessTokenConsumer* consumer) override {
    num_access_token_fetches_++;
    last_access_token_account_key_ = account_key;

    return account_manager::AccountManager::CreateAccessTokenFetcher(
        account_key, consumer);
  }

  int num_access_token_fetches() const { return num_access_token_fetches_; }

  account_manager::AccountKey last_access_token_account_key() const {
    return last_access_token_account_key_.value();
  }

 private:
  // Mutated by const CreateAccessTokenFetcher.
  mutable int num_access_token_fetches_ = 0;
  mutable std::optional<account_manager::AccountKey>
      last_access_token_account_key_;
};

class AccountManagerMojoServiceTest : public ::testing::Test {
 public:
  AccountManagerMojoServiceTest() = default;
  AccountManagerMojoServiceTest(const AccountManagerMojoServiceTest&) = delete;
  AccountManagerMojoServiceTest& operator=(
      const AccountManagerMojoServiceTest&) = delete;
  ~AccountManagerMojoServiceTest() override = default;

 protected:
  void SetUp() override {
    account_manager_mojo_service_ =
        std::make_unique<AccountManagerMojoService>(&account_manager_);
    account_manager_mojo_service_->SetAccountManagerUI(
        std::make_unique<FakeAccountManagerUI>());
    account_manager_mojo_service_->BindReceiver(
        remote_.BindNewPipeAndPassReceiver());
    account_manager_async_waiter_ =
        std::make_unique<mojom::AccountManagerAsyncWaiter>(
            account_manager_mojo_service_.get());
  }

  void RunAllPendingTasks() { task_environment_.RunUntilIdle(); }

  void FlushMojoForTesting() {
    account_manager_mojo_service_->FlushMojoForTesting();
  }

  // Returns `true` if initialization was successful.
  bool InitializeAccountManager() {
    base::test::TestFuture<void> future;
    account_manager_.InitializeInEphemeralMode(
        test_url_loader_factory_.GetSafeWeakWrapper(), future.GetCallback());
    account_manager_.SetPrefService(&pref_service_);
    account_manager_.RegisterPrefs(pref_service_.registry());
    EXPECT_TRUE(future.Wait());
    return account_manager_.IsInitialized();
  }

  FakeAccountManagerUI* GetFakeAccountManagerUI() {
    return static_cast<FakeAccountManagerUI*>(
        account_manager_mojo_service_->account_manager_ui_.get());
  }

  void ShowAddAccountDialog(
      crosapi::mojom::AccountAdditionOptionsPtr options,
      AccountManagerMojoService::ShowAddAccountDialogCallback callback) {
    account_manager_mojo_service_->ShowAddAccountDialog(std::move(options),
                                                        std::move(callback));
  }

  void ShowAddAccountDialog(
      account_manager::AccountAdditionSource source,
      crosapi::mojom::AccountAdditionOptionsPtr options,
      AccountManagerMojoService::ShowAddAccountDialogCallback callback) {
    account_manager_mojo_service_->ShowAddAccountDialog(
        source, std::move(options), std::move(callback));
  }

  void ShowReauthAccountDialog(
      const std::string& email,
      AccountManagerMojoService::ShowReauthAccountDialogCallback callback) {
    account_manager_mojo_service_->ShowReauthAccountDialog(email,
                                                           std::move(callback));
  }

  void ShowReauthAccountDialog(
      account_manager::AccountAdditionSource source,
      const std::string& email,
      AccountManagerMojoService::ShowReauthAccountDialogCallback callback) {
    account_manager_mojo_service_->ShowReauthAccountDialog(source, email,
                                                           std::move(callback));
  }

  void CallAccountUpsertionFinished(
      const account_manager::AccountUpsertionResult& result) {
    account_manager_mojo_service_
        ->CreateInlineLoginAccountUpsertionFinishedCallback()
        .Run(result);
    GetFakeAccountManagerUI()->CloseDialog();
  }

  mojom::AccessTokenResultPtr FetchAccessToken(
      const account_manager::AccountKey& account_key) {
    return FetchAccessToken(account_key, /*scopes=*/{});
  }

  mojom::AccessTokenResultPtr FetchAccessToken(
      const account_manager::AccountKey& account_key,
      const std::vector<std::string>& scopes) {
    mojo::PendingRemote<mojom::AccessTokenFetcher> pending_remote;
    account_manager_async_waiter()->CreateAccessTokenFetcher(
        account_manager::ToMojoAccountKey(account_key), kFakeOAuthConsumerName,
        &pending_remote);
    mojo::Remote<mojom::AccessTokenFetcher> remote(std::move(pending_remote));

    base::test::TestFuture<mojom::AccessTokenResultPtr> future;
    remote->Start(scopes, future.GetCallback());
    return future.Take();
  }

  void AddFakeAccessTokenResponse() {
    GURL url(GaiaUrls::GetInstance()->oauth2_token_url());
    test_url_loader_factory_.AddResponse(url.spec(), kAccessTokenResponse,
                                         net::HTTP_OK);
  }

  int GetNumObservers() const {
    return account_manager_mojo_service_->observers_.size();
  }

  int GetNumPendingAccessTokenRequests() const {
    return account_manager_mojo_service_->GetNumPendingAccessTokenRequests();
  }

  mojom::AccountManagerAsyncWaiter* account_manager_async_waiter() {
    return account_manager_async_waiter_.get();
  }

  AccountManagerSpy* account_manager() { return &account_manager_; }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  TestingPrefServiceSimple pref_service_;
  AccountManagerSpy account_manager_;
  mojo::Remote<mojom::AccountManager> remote_;
  std::unique_ptr<AccountManagerMojoService> account_manager_mojo_service_;
  std::unique_ptr<mojom::AccountManagerAsyncWaiter>
      account_manager_async_waiter_;
};

// Test that lacros remotes do not leak.
TEST_F(AccountManagerMojoServiceTest,
       LacrosRemotesAreAutomaticallyRemovedOnConnectionClose) {
  EXPECT_EQ(0, GetNumObservers());
  {
    mojo::PendingReceiver<mojom::AccountManagerObserver> receiver;
    account_manager_async_waiter()->AddObserver(&receiver);
    EXPECT_EQ(1, GetNumObservers());
  }
  // Wait for the disconnect handler to be called.
  RunAllPendingTasks();
  EXPECT_EQ(0, GetNumObservers());
}

TEST_F(AccountManagerMojoServiceTest,
       ShowAddAccountDialogReturnsInProgressIfDialogIsOpen) {
  EXPECT_EQ(0, GetFakeAccountManagerUI()->show_account_addition_dialog_calls());
  GetFakeAccountManagerUI()->SetIsDialogShown(true);
  mojom::AccountUpsertionResultPtr account_upsertion_result;
  account_manager_async_waiter()->ShowAddAccountDialog(
      crosapi::mojom::AccountAdditionOptions::New(), &account_upsertion_result);

  // Check status.
  EXPECT_EQ(mojom::AccountUpsertionResult::Status::kAlreadyInProgress,
            account_upsertion_result->status);
  // Check that dialog was not called.
  EXPECT_EQ(0, GetFakeAccountManagerUI()->show_account_addition_dialog_calls());
}

TEST_F(AccountManagerMojoServiceTest,
       ShowReauthAccountDialogReturnsInProgressIfDialogIsOpen) {
  EXPECT_EQ(
      0,
      GetFakeAccountManagerUI()->show_account_reauthentication_dialog_calls());
  GetFakeAccountManagerUI()->SetIsDialogShown(true);
  base::test::TestFuture<mojom::AccountUpsertionResultPtr> future;
  ShowReauthAccountDialog(kFakeEmail, future.GetCallback());
  auto result = future.Take();

  // Check status.
  EXPECT_EQ(mojom::AccountUpsertionResult::Status::kAlreadyInProgress,
            result->status);
  // Check that dialog was not called.
  EXPECT_EQ(
      0,
      GetFakeAccountManagerUI()->show_account_reauthentication_dialog_calls());
}

TEST_F(AccountManagerMojoServiceTest,
       ShowAddAccountDialogReturnsCancelledAfterDialogIsClosed) {
  EXPECT_EQ(0, GetFakeAccountManagerUI()->show_account_addition_dialog_calls());
  GetFakeAccountManagerUI()->SetIsDialogShown(false);

  base::test::TestFuture<mojom::AccountUpsertionResultPtr> future;
  ShowAddAccountDialog(crosapi::mojom::AccountAdditionOptions::New(),
                       future.GetCallback());

  // Simulate closing the dialog.
  GetFakeAccountManagerUI()->CloseDialog();
  auto result = future.Take();
  // Check status.
  EXPECT_EQ(mojom::AccountUpsertionResult::Status::kCancelledByUser,
            result->status);
  // Check that dialog was called once.
  EXPECT_EQ(1, GetFakeAccountManagerUI()->show_account_addition_dialog_calls());
}

TEST_F(AccountManagerMojoServiceTest,
       ShowAddAccountDialogRecordsSourceAndResultUMA) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<mojom::AccountUpsertionResultPtr> future;
  ShowAddAccountDialog(account_manager::AccountAdditionSource::kArc,
                       crosapi::mojom::AccountAdditionOptions::New(),
                       future.GetCallback());

  GetFakeAccountManagerUI()->CloseDialog();
  EXPECT_EQ(mojom::AccountUpsertionResult::Status::kCancelledByUser,
            future.Take()->status);
  histogram_tester.ExpectUniqueSample(
      account_manager::kAccountAdditionSourceHistogramName,
      account_manager::AccountAdditionSource::kArc, 1);
  histogram_tester.ExpectUniqueSample(
      account_manager::kAccountUpsertionResultStatusHistogramName,
      account_manager::AccountUpsertionResult::Status::kCancelledByUser, 1);
}

TEST_F(AccountManagerMojoServiceTest,
       ShowAddAccountDialogReturnsSuccessAfterAccountIsAdded) {
  EXPECT_EQ(0, GetFakeAccountManagerUI()->show_account_addition_dialog_calls());
  GetFakeAccountManagerUI()->SetIsDialogShown(false);

  base::test::TestFuture<mojom::AccountUpsertionResultPtr> future;
  ShowAddAccountDialog(crosapi::mojom::AccountAdditionOptions::New(),
                       future.GetCallback());
  // Simulate account addition.
  CallAccountUpsertionFinished(
      account_manager::AccountUpsertionResult::FromAccount(kFakeAccount));
  // Simulate closing the dialog.
  GetFakeAccountManagerUI()->CloseDialog();
  auto result = future.Take();

  // Check status.
  EXPECT_EQ(mojom::AccountUpsertionResult::Status::kSuccess, result->status);
  // Check account.
  std::optional<account_manager::Account> account =
      account_manager::FromMojoAccount(result->account);
  EXPECT_TRUE(account.has_value());
  EXPECT_EQ(kFakeAccount.key, account.value().key);
  EXPECT_EQ(kFakeAccount.raw_email, account.value().raw_email);
  // Check that dialog was called once.
  EXPECT_EQ(1, GetFakeAccountManagerUI()->show_account_addition_dialog_calls());
}

TEST_F(AccountManagerMojoServiceTest,
       ShowReauthAccountDialogRecordsSourceAndResultUMA) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<mojom::AccountUpsertionResultPtr> future;
  ShowReauthAccountDialog(
      account_manager::AccountAdditionSource::kChromeOSProjectorAppReauth,
      kFakeEmail, future.GetCallback());

  GetFakeAccountManagerUI()->CloseDialog();
  EXPECT_EQ(mojom::AccountUpsertionResult::Status::kCancelledByUser,
            future.Take()->status);
  histogram_tester.ExpectUniqueSample(
      account_manager::kAccountAdditionSourceHistogramName,
      account_manager::AccountAdditionSource::kChromeOSProjectorAppReauth, 1);
  histogram_tester.ExpectUniqueSample(
      account_manager::kAccountUpsertionResultStatusHistogramName,
      account_manager::AccountUpsertionResult::Status::kCancelledByUser, 1);
}

TEST_F(AccountManagerMojoServiceTest,
       ShowReauthAccountDialogReturnsSuccessAfterAccountIsAdded) {
  EXPECT_EQ(
      0,
      GetFakeAccountManagerUI()->show_account_reauthentication_dialog_calls());
  GetFakeAccountManagerUI()->SetIsDialogShown(false);

  base::test::TestFuture<mojom::AccountUpsertionResultPtr> future;
  ShowReauthAccountDialog(kFakeEmail, future.GetCallback());
  // Simulate account reauth.
  CallAccountUpsertionFinished(
      account_manager::AccountUpsertionResult::FromAccount(kFakeAccount));
  // Simulate closing the dialog.
  GetFakeAccountManagerUI()->CloseDialog();
  auto result = future.Take();

  // Check status.
  EXPECT_EQ(mojom::AccountUpsertionResult::Status::kSuccess, result->status);
  // Check account.
  std::optional<account_manager::Account> account =
      account_manager::FromMojoAccount(result->account);
  EXPECT_TRUE(account.has_value());
  EXPECT_EQ(kFakeAccount.key, account.value().key);
  EXPECT_EQ(kFakeAccount.raw_email, account.value().raw_email);
  // Check that dialog was called once.
  EXPECT_EQ(
      1,
      GetFakeAccountManagerUI()->show_account_reauthentication_dialog_calls());
}

TEST_F(AccountManagerMojoServiceTest,
       ShowAddAccountDialogCanHandleMultipleCalls) {
  EXPECT_EQ(0, GetFakeAccountManagerUI()->show_account_addition_dialog_calls());
  GetFakeAccountManagerUI()->SetIsDialogShown(false);

  base::test::TestFuture<mojom::AccountUpsertionResultPtr> future;
  ShowAddAccountDialog(crosapi::mojom::AccountAdditionOptions::New(),
                       future.GetCallback());

  base::test::TestFuture<mojom::AccountUpsertionResultPtr> future_2;
  ShowAddAccountDialog(crosapi::mojom::AccountAdditionOptions::New(),
                       future_2.GetCallback());
  auto result_2 = future_2.Take();
  // The second call gets 'kAlreadyInProgress' reply.
  EXPECT_EQ(mojom::AccountUpsertionResult::Status::kAlreadyInProgress,
            result_2->status);

  // Simulate account addition.
  CallAccountUpsertionFinished(
      account_manager::AccountUpsertionResult::FromAccount(kFakeAccount));
  // Simulate closing the dialog.
  GetFakeAccountManagerUI()->CloseDialog();
  auto result = future.Take();

  EXPECT_EQ(mojom::AccountUpsertionResult::Status::kSuccess, result->status);
  // Check account.
  std::optional<account_manager::Account> account =
      account_manager::FromMojoAccount(result->account);
  EXPECT_TRUE(account.has_value());
  EXPECT_EQ(kFakeAccount.key, account.value().key);
  EXPECT_EQ(kFakeAccount.raw_email, account.value().raw_email);
  // Check that dialog was called once.
  EXPECT_EQ(1, GetFakeAccountManagerUI()->show_account_addition_dialog_calls());
}

TEST_F(AccountManagerMojoServiceTest,
       ShowAddAccountDialogCanHandleMultipleSequentialCalls) {
  EXPECT_EQ(0, GetFakeAccountManagerUI()->show_account_addition_dialog_calls());
  GetFakeAccountManagerUI()->SetIsDialogShown(false);

  base::test::TestFuture<mojom::AccountUpsertionResultPtr> future;
  ShowAddAccountDialog(crosapi::mojom::AccountAdditionOptions::New(),
                       future.GetCallback());
  // Simulate account addition.
  CallAccountUpsertionFinished(
      account_manager::AccountUpsertionResult::FromAccount(kFakeAccount));
  // Simulate closing the dialog.
  GetFakeAccountManagerUI()->CloseDialog();
  auto result = future.Take();
  EXPECT_EQ(mojom::AccountUpsertionResult::Status::kSuccess, result->status);
  // Check account.
  std::optional<account_manager::Account> account =
      account_manager::FromMojoAccount(result->account);
  EXPECT_TRUE(account.has_value());
  EXPECT_EQ(kFakeAccount.key, account.value().key);
  EXPECT_EQ(kFakeAccount.raw_email, account.value().raw_email);

  base::test::TestFuture<mojom::AccountUpsertionResultPtr> future_2;
  ShowAddAccountDialog(crosapi::mojom::AccountAdditionOptions::New(),
                       future_2.GetCallback());
  // Simulate account addition.
  CallAccountUpsertionFinished(
      account_manager::AccountUpsertionResult::FromAccount(kFakeAccount));
  // Simulate closing the dialog.
  GetFakeAccountManagerUI()->CloseDialog();
  auto result_2 = future_2.Take();
  EXPECT_EQ(mojom::AccountUpsertionResult::Status::kSuccess, result_2->status);
  // Check account.
  std::optional<account_manager::Account> account_2 =
      account_manager::FromMojoAccount(result_2->account);
  EXPECT_TRUE(account_2.has_value());
  EXPECT_EQ(kFakeAccount.key, account_2.value().key);
  EXPECT_EQ(kFakeAccount.raw_email, account_2.value().raw_email);

  // Check that dialog was called 2 times.
  EXPECT_EQ(2, GetFakeAccountManagerUI()->show_account_addition_dialog_calls());
}

TEST_F(AccountManagerMojoServiceTest,
       ShowReauthAccountDialogDoesntCallTheDialogIfItsAlreadyShown) {
  EXPECT_EQ(
      0,
      GetFakeAccountManagerUI()->show_account_reauthentication_dialog_calls());
  GetFakeAccountManagerUI()->SetIsDialogShown(true);
  base::test::TestFuture<mojom::AccountUpsertionResultPtr> future;
  // Simulate account re-authentication.
  ShowReauthAccountDialog(kFakeEmail, future.GetCallback());
  // Simulate closing the dialog.
  GetFakeAccountManagerUI()->CloseDialog();

  // Check that dialog was not called.
  EXPECT_EQ(
      0,
      GetFakeAccountManagerUI()->show_account_reauthentication_dialog_calls());
}

TEST_F(AccountManagerMojoServiceTest, ShowReauthAccountDialogOpensTheDialog) {
  EXPECT_EQ(
      0,
      GetFakeAccountManagerUI()->show_account_reauthentication_dialog_calls());
  GetFakeAccountManagerUI()->SetIsDialogShown(false);

  // Simulate account reauthentication.
  base::test::TestFuture<mojom::AccountUpsertionResultPtr> future;
  ShowReauthAccountDialog(kFakeEmail, future.GetCallback());
  // Simulate closing the dialog.
  GetFakeAccountManagerUI()->CloseDialog();
  auto result = future.Take();

  // Check status.
  EXPECT_EQ(mojom::AccountUpsertionResult::Status::kCancelledByUser,
            result->status);
  // Check that dialog was called once.
  EXPECT_EQ(
      1,
      GetFakeAccountManagerUI()->show_account_reauthentication_dialog_calls());
}

TEST_F(AccountManagerMojoServiceTest,
       FetchingAccessTokenResultsInErrorForUnknownAccountKey) {
  ASSERT_TRUE(InitializeAccountManager());
  EXPECT_EQ(0, GetNumPendingAccessTokenRequests());
  const account_manager::AccountKey account_key =
      account_manager::AccountKey::FromGaiaId(kFakeGaiaId);
  mojom::AccessTokenResultPtr result = FetchAccessToken(account_key);

  ASSERT_TRUE(result->is_error());
  EXPECT_EQ(mojom::GoogleServiceAuthError::State::kAccountNotFound,
            result->get_error()->state);

  // Check that requests are not leaking.
  RunAllPendingTasks();
  EXPECT_EQ(0, GetNumPendingAccessTokenRequests());
}

TEST_F(AccountManagerMojoServiceTest, FetchAccessTokenRequestsCanBeCancelled) {
  // Setup.
  ASSERT_TRUE(InitializeAccountManager());
  const account_manager::AccountKey account_key =
      account_manager::AccountKey::FromGaiaId(kFakeGaiaId);
  account_manager()->UpsertAccount(account_key, kFakeEmail, kFakeToken);
  mojo::PendingRemote<mojom::AccessTokenFetcher> pending_remote;
  EXPECT_EQ(0, GetNumPendingAccessTokenRequests());
  account_manager_async_waiter()->CreateAccessTokenFetcher(
      account_manager::ToMojoAccountKey(account_key), kFakeOAuthConsumerName,
      &pending_remote);
  mojo::Remote<mojom::AccessTokenFetcher> remote(std::move(pending_remote));
  mojom::AccessTokenResultPtr result;
  EXPECT_TRUE(result.is_null());
  // Make a request to fetch access token. Since we haven't setup our test URL
  // loader factory via `AddFakeAccessTokenResponse`, this request will never be
  // completed.
  remote->Start(/*scopes=*/{},
                base::BindLambdaForTesting(
                    [&result](mojom::AccessTokenResultPtr returned_result) {
                      result = std::move(returned_result);
                    }));
  EXPECT_EQ(1, GetNumPendingAccessTokenRequests());

  // Test.
  // This should cancel the pending request.
  remote.reset();
  RunAllPendingTasks();
  EXPECT_EQ(0, GetNumPendingAccessTokenRequests());
  // Verify that result is still unset - i.e. the pending request was cancelled,
  // and didn't complete normally.
  EXPECT_TRUE(result.is_null());
}

TEST_F(AccountManagerMojoServiceTest, FetchAccessToken) {
  constexpr char kFakeScope[] = "fake-scope";
  ASSERT_TRUE(InitializeAccountManager());
  const account_manager::AccountKey account_key =
      account_manager::AccountKey::FromGaiaId(kFakeGaiaId);
  account_manager()->UpsertAccount(account_key, kFakeEmail, kFakeToken);
  AddFakeAccessTokenResponse();
  EXPECT_EQ(0, GetNumPendingAccessTokenRequests());
  mojom::AccessTokenResultPtr result =
      FetchAccessToken(account_key, {kFakeScope});

  ASSERT_TRUE(result->is_access_token_info());
  EXPECT_EQ(kFakeAccessToken, result->get_access_token_info()->access_token);
  EXPECT_EQ(1, account_manager()->num_access_token_fetches());
  EXPECT_EQ(account_key, account_manager()->last_access_token_account_key());

  // Check that requests are not leaking.
  RunAllPendingTasks();
  EXPECT_EQ(0, GetNumPendingAccessTokenRequests());
}

TEST_F(AccountManagerMojoServiceTest,
       ObserversAreNotifiedAboutAccountAdditionDialogClosure) {
  TestAccountManagerObserver observer;
  observer.Observe(account_manager_async_waiter());
  ASSERT_EQ(1, GetNumObservers());

  EXPECT_EQ(0, observer.GetNumSigninDialogClosedNotifications());
  GetFakeAccountManagerUI()->SetIsDialogShown(false);
  base::test::TestFuture<mojom::AccountUpsertionResultPtr> future;
  ShowAddAccountDialog(crosapi::mojom::AccountAdditionOptions::New(),
                       future.GetCallback());
  // Simulate closing the dialog.
  GetFakeAccountManagerUI()->CloseDialog();
  auto result = future.Take();
  EXPECT_EQ(mojom::AccountUpsertionResult::Status::kCancelledByUser,
            result->status);
  FlushMojoForTesting();
  EXPECT_EQ(1, observer.GetNumSigninDialogClosedNotifications());
}

TEST_F(AccountManagerMojoServiceTest,
       ObserversAreNotifiedAboutReautDialogClosure) {
  TestAccountManagerObserver observer;
  observer.Observe(account_manager_async_waiter());
  ASSERT_EQ(1, GetNumObservers());

  EXPECT_EQ(0, observer.GetNumSigninDialogClosedNotifications());
  GetFakeAccountManagerUI()->SetIsDialogShown(false);
  base::test::TestFuture<mojom::AccountUpsertionResultPtr> future;
  ShowReauthAccountDialog(kFakeEmail, future.GetCallback());
  // Simulate closing the dialog.
  GetFakeAccountManagerUI()->CloseDialog();
  auto result = future.Take();
  EXPECT_EQ(mojom::AccountUpsertionResult::Status::kCancelledByUser,
            result->status);
  FlushMojoForTesting();
  EXPECT_EQ(1, observer.GetNumSigninDialogClosedNotifications());
}

}  // namespace crosapi
