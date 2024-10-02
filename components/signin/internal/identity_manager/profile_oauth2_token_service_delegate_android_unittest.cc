// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/profile_oauth2_token_service_delegate_android.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"

#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::Pointwise;
using ::testing::Return;
using ::testing::Sequence;
using ::testing::StrictMock;

namespace signin {

namespace {
const std::vector<CoreAccountId> kEmptyVector;
class OAuth2TokenServiceDelegateAndroidForTest
    : public ProfileOAuth2TokenServiceDelegateAndroid {
 public:
  OAuth2TokenServiceDelegateAndroidForTest(
      AccountTrackerService* account_tracker_service)
      : ProfileOAuth2TokenServiceDelegateAndroid(account_tracker_service) {}
  MOCK_METHOD1(SetAccounts, void(const std::vector<CoreAccountId>&));
};

class TestObserver : public ProfileOAuth2TokenServiceObserver {
 public:
  MOCK_METHOD1(OnRefreshTokenAvailable, void(const CoreAccountId&));
  MOCK_METHOD1(OnRefreshTokenRevoked, void(const CoreAccountId&));
  MOCK_METHOD0(OnRefreshTokensLoaded, void());
};

MATCHER(CoreAccountInfoEq,
        /* std::tuple<const AccountInfo&, const AccountInfo&> arg, */
        "") {
  return static_cast<const CoreAccountInfo&>(std::get<0>(arg)) ==
         static_cast<const CoreAccountInfo&>(std::get<1>(arg));
}
}  // namespace

class OAuth2TokenServiceDelegateAndroidTest : public testing::Test {
 public:
  OAuth2TokenServiceDelegateAndroidTest() {}
  ~OAuth2TokenServiceDelegateAndroidTest() override = default;

 protected:
  void SetUp() override {
    testing::Test::SetUp();
    AccountTrackerService::RegisterPrefs(pref_service_.registry());
    account_tracker_service_.Initialize(&pref_service_, base::FilePath());
    SetUpMockAccountManagerFacade();
    delegate_ = std::make_unique<OAuth2TokenServiceDelegateAndroidForTest>(
        &account_tracker_service_);
    delegate_->AddObserver(&observer_);
    CreateAndSeedAccounts();
  }

  void TearDown() override {
    delegate_->RemoveObserver(&observer_);
    testing::Test::TearDown();
  }

  AccountInfo CreateAccountInfo(const std::string& gaia_id,
                                const std::string& email) {
    AccountInfo account_info;

    account_info.gaia = gaia_id;
    account_info.email = email;
    account_info.full_name = "fullname";
    account_info.given_name = "givenname";
    account_info.hosted_domain = "example.com";
    account_info.locale = "en";
    account_info.picture_url = "https://example.com";
    account_info.account_id = account_tracker_service_.PickAccountIdForAccount(
        account_info.gaia, account_info.email);

    DCHECK(account_info.IsValid());

    return account_info;
  }

  void CreateAndSeedAccounts() {
    account1_ = CreateAccountInfo("gaia-id-user-1", "user-1@example.com");
    account2_ = CreateAccountInfo("gaia-id-user-2", "user-2@example.com");
    // SeedAccountInfo is required for
    // OAuth2TokenServiceDelegateAndrod::MapAccountNameToAccountId
    account_tracker_service_.SeedAccountInfo(account1_);
    account_tracker_service_.SeedAccountInfo(account2_);
  }

  AccountTrackerService account_tracker_service_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  std::unique_ptr<OAuth2TokenServiceDelegateAndroidForTest> delegate_;
  StrictMock<TestObserver> observer_;

  AccountInfo account1_;
  AccountInfo account2_;
};

TEST_F(OAuth2TokenServiceDelegateAndroidTest,
       UpdateAccountListWith0SystemAccount0AccountAndNotSignedIn) {
  EXPECT_CALL(*delegate_, SetAccounts(kEmptyVector)).WillOnce(Return());
  // No observer call expected
  delegate_->UpdateAccountList(CoreAccountId(), {}, {});
  EXPECT_TRUE(account_tracker_service_.GetAccounts().empty());
}

TEST_F(OAuth2TokenServiceDelegateAndroidTest,
       UpdateAccountListWith1SystemAccount0AccountAndNotSignedIn) {
  EXPECT_CALL(*delegate_, SetAccounts(kEmptyVector)).WillOnce(Return());
  // No observer call expected
  delegate_->UpdateAccountList(CoreAccountId(), {}, {account1_.account_id});
  EXPECT_THAT(
      std::vector<AccountInfo>{account1_},
      Pointwise(CoreAccountInfoEq(), account_tracker_service_.GetAccounts()));
}

TEST_F(OAuth2TokenServiceDelegateAndroidTest,
       UpdateAccountListWith1SystemAccount1AccountAndNotSignedIn) {
  Sequence seq;
  EXPECT_CALL(*delegate_, SetAccounts(kEmptyVector))
      .InSequence(seq)
      .WillOnce(Return());
  // Stored account from |GetAccounts| must fire a revoked event
  EXPECT_CALL(observer_, OnRefreshTokenRevoked(account1_.account_id))
      .InSequence(seq)
      .WillOnce(Return());

  delegate_->UpdateAccountList(CoreAccountId(), {account1_.account_id},
                               {account1_.account_id});
  EXPECT_THAT(
      std::vector<AccountInfo>{account1_},
      Pointwise(CoreAccountInfoEq(), account_tracker_service_.GetAccounts()));
}

TEST_F(OAuth2TokenServiceDelegateAndroidTest,
       UpdateAccountListWith1SystemAccount0AccountAndSignedIn) {
  Sequence seq;
  EXPECT_CALL(*delegate_,
              SetAccounts(std::vector<CoreAccountId>({account1_.account_id})))
      .InSequence(seq)
      .WillOnce(Return());
  EXPECT_CALL(observer_, OnRefreshTokenAvailable(account1_.account_id))
      .InSequence(seq)
      .WillOnce(Return());

  delegate_->UpdateAccountList(account1_.account_id, {},
                               {account1_.account_id});
  EXPECT_THAT(
      std::vector<AccountInfo>{account1_},
      Pointwise(CoreAccountInfoEq(), account_tracker_service_.GetAccounts()));
}

TEST_F(OAuth2TokenServiceDelegateAndroidTest,
       UpdateAccountListWith1SystemAccount1AccountAndSignedIn) {
  Sequence seq;
  EXPECT_CALL(*delegate_,
              SetAccounts(std::vector<CoreAccountId>({account1_.account_id})))
      .InSequence(seq)
      .WillOnce(Return());
  EXPECT_CALL(observer_, OnRefreshTokenAvailable(account1_.account_id))
      .InSequence(seq)
      .WillOnce(Return());

  delegate_->UpdateAccountList(account1_.account_id, {account1_.account_id},
                               {account1_.account_id});
  EXPECT_THAT(
      std::vector<AccountInfo>{account1_},
      Pointwise(CoreAccountInfoEq(), account_tracker_service_.GetAccounts()));
}

TEST_F(OAuth2TokenServiceDelegateAndroidTest,
       UpdateAccountListWith1SystemAccount1AccountDifferentAndSignedIn) {
  Sequence seq;
  EXPECT_CALL(*delegate_,
              SetAccounts(std::vector<CoreAccountId>({account1_.account_id})))
      .InSequence(seq)
      .WillOnce(Return());
  // Previously stored account is removed, new account is available
  EXPECT_CALL(observer_, OnRefreshTokenAvailable(account1_.account_id))
      .InSequence(seq)
      .WillOnce(Return());
  EXPECT_CALL(observer_, OnRefreshTokenRevoked(account2_.account_id))
      .InSequence(seq)
      .WillOnce(Return());

  delegate_->UpdateAccountList(account1_.account_id, {account2_.account_id},
                               {account1_.account_id});
  EXPECT_THAT(
      std::vector<AccountInfo>{account1_},
      Pointwise(CoreAccountInfoEq(), account_tracker_service_.GetAccounts()));
}

TEST_F(OAuth2TokenServiceDelegateAndroidTest,
       UpdateAccountListWith0SystemAccount1AccountSignedIn) {
  Sequence seq;
  EXPECT_CALL(*delegate_, SetAccounts(kEmptyVector))
      .InSequence(seq)
      .WillOnce(Return());
  EXPECT_CALL(observer_, OnRefreshTokenRevoked(account1_.account_id))
      .InSequence(seq)
      .WillOnce(Return());

  delegate_->UpdateAccountList(account1_.account_id, {account1_.account_id},
                               {});
  EXPECT_TRUE(account_tracker_service_.GetAccounts().empty());
}

TEST_F(OAuth2TokenServiceDelegateAndroidTest,
       UpdateAccountListWith1SystemAccount0AccountAndSignedInDifferent) {
  EXPECT_CALL(*delegate_, SetAccounts(kEmptyVector)).WillOnce(Return());

  delegate_->UpdateAccountList(account2_.account_id, {},
                               {account1_.account_id});
  EXPECT_THAT(
      std::vector<AccountInfo>{account1_},
      Pointwise(CoreAccountInfoEq(), account_tracker_service_.GetAccounts()));
}

// Test Getsysaccounts return a user != from signed user while GetAccounts not
// empty
TEST_F(OAuth2TokenServiceDelegateAndroidTest,
       UpdateAccountListWith1SystemAccount1AccountAndSignedInDifferent) {
  Sequence seq;
  EXPECT_CALL(*delegate_, SetAccounts(kEmptyVector))
      .InSequence(seq)
      .WillOnce(Return());
  EXPECT_CALL(observer_, OnRefreshTokenRevoked(account1_.account_id))
      .InSequence(seq)
      .WillOnce(Return());

  delegate_->UpdateAccountList(account2_.account_id, {account1_.account_id},
                               {account1_.account_id});
  EXPECT_THAT(
      std::vector<AccountInfo>{account1_},
      Pointwise(CoreAccountInfoEq(), account_tracker_service_.GetAccounts()));
}

TEST_F(OAuth2TokenServiceDelegateAndroidTest,
       UpdateAccountListWith2SystemAccount0AccountAndSignedIn) {
  Sequence seq;
  EXPECT_CALL(*delegate_, SetAccounts(std::vector<CoreAccountId>(
                              {account1_.account_id, account2_.account_id})))
      .InSequence(seq)
      .WillOnce(Return());
  // OnRefreshTokenAvailable fired, signed in account should go first.
  EXPECT_CALL(observer_, OnRefreshTokenAvailable(account2_.account_id))
      .InSequence(seq)
      .WillOnce(Return());
  EXPECT_CALL(observer_, OnRefreshTokenAvailable(account1_.account_id))
      .InSequence(seq)
      .WillOnce(Return());

  delegate_->UpdateAccountList(account2_.account_id, {},
                               {account1_.account_id, account2_.account_id});
  EXPECT_THAT(
      std::vector<AccountInfo>({account1_, account2_}),
      Pointwise(CoreAccountInfoEq(), account_tracker_service_.GetAccounts()));
}

TEST_F(OAuth2TokenServiceDelegateAndroidTest,
       UpdateAccountListWith2SystemAccount1AccountAndSignedIn) {
  Sequence seq;
  EXPECT_CALL(*delegate_, SetAccounts(std::vector<CoreAccountId>(
                              {account1_.account_id, account2_.account_id})))
      .InSequence(seq)
      .WillOnce(Return());
  // OnRefreshTokenAvailable fired, signed in account should go first.
  EXPECT_CALL(observer_, OnRefreshTokenAvailable(account1_.account_id))
      .InSequence(seq)
      .WillOnce(Return());
  EXPECT_CALL(observer_, OnRefreshTokenAvailable(account2_.account_id))
      .InSequence(seq)
      .WillOnce(Return());
  delegate_->UpdateAccountList(account1_.account_id, {account2_.account_id},
                               {account1_.account_id, account2_.account_id});
  EXPECT_THAT(
      std::vector<AccountInfo>({account1_, account2_}),
      Pointwise(CoreAccountInfoEq(), account_tracker_service_.GetAccounts()));
}

TEST_F(OAuth2TokenServiceDelegateAndroidTest,
       UpdateAccountListWith1SystemAccount2AccountAndSignedIn) {
  Sequence seq;
  EXPECT_CALL(*delegate_,
              SetAccounts(std::vector<CoreAccountId>({account1_.account_id})))
      .InSequence(seq)
      .WillOnce(Return());
  // OnRefreshTokenAvailable fired, signed in account should go first.
  EXPECT_CALL(observer_, OnRefreshTokenAvailable(account1_.account_id))
      .InSequence(seq)
      .WillOnce(Return());
  EXPECT_CALL(observer_, OnRefreshTokenRevoked(account2_.account_id))
      .InSequence(seq)
      .WillOnce(Return());

  delegate_->UpdateAccountList(account1_.account_id,
                               {account1_.account_id, account2_.account_id},
                               {account1_.account_id});
  EXPECT_THAT(
      std::vector<AccountInfo>({account1_}),
      Pointwise(CoreAccountInfoEq(), account_tracker_service_.GetAccounts()));
}

}  // namespace signin
