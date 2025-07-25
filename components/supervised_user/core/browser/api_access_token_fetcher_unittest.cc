// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/api_access_token_fetcher.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/supervised_user/core/browser/fetcher_config.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace supervised_user {
namespace {

using ::base::BindOnce;
using ::base::expected;
using ::base::OnceCallback;
using ::base::Time;
using ::base::unexpected;
using ::base::Unretained;
using ::base::test::TaskEnvironment;
using ::signin::AccessTokenFetcher;
using ::signin::AccessTokenInfo;
using ::signin::ConsentLevel;
using ::signin::IdentityTestEnvironment;

class ApiAccessTokenFetcherTest
    : public ::testing::TestWithParam<AccessTokenConfig> {
 protected:
  // A pinhole class that allows to verify the fetch result.
  class Receiver {
   public:
    using FetchResultType = expected<AccessTokenInfo, GoogleServiceAuthError>;
    // The closure is safe to use if `this` pointer outlives its ::Run.
    ApiAccessTokenFetcher::Consumer Receive() {
      return BindOnce(&Receiver::Set, Unretained(this));
    }

    const FetchResultType& Get() const { return fetch_result_; }

   private:
    void Set(FetchResultType fetch_result) { fetch_result_ = fetch_result; }
    FetchResultType fetch_result_ = unexpected(GoogleServiceAuthError());
  };

  AccessTokenConfig GetAccessTokenConfig() { return GetParam(); }
  TaskEnvironment task_environment_;
  IdentityTestEnvironment identity_test_env_;
};

TEST_P(ApiAccessTokenFetcherTest, ReadToken) {
  AccountInfo account = identity_test_env_.MakePrimaryAccountAvailable(
      "bob@example.com", ConsentLevel::kSignin);
  Receiver receiver;
  ApiAccessTokenFetcher service(*identity_test_env_.identity_manager(),
                                GetAccessTokenConfig(), receiver.Receive());

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "expected_access_token", Time::Max());

  EXPECT_EQ(receiver.Get().value().token, "expected_access_token");
}

TEST_P(ApiAccessTokenFetcherTest, AuthError) {
  AccountInfo account = identity_test_env_.MakePrimaryAccountAvailable(
      "bob@example.com", ConsentLevel::kSignin);
  Receiver receiver;
  ApiAccessTokenFetcher service(*identity_test_env_.identity_manager(),
                                GetAccessTokenConfig(), receiver.Receive());

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(
          GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS));

  EXPECT_EQ(receiver.Get().error().state(),
            GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS);
}

INSTANTIATE_TEST_SUITE_P(
    ApiAccessTokenFetcherTest,
    ApiAccessTokenFetcherTest,
    ::testing::Values(kClassifyUrlConfig.access_token_config,
                      kListFamilyMembersConfig.access_token_config));

}  // namespace
}  // namespace supervised_user
