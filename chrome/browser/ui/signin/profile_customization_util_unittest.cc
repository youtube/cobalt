// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/signin/profile_customization_util.h"

#include <string>

#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

AccountInfo FillAccountInfo(
    const CoreAccountInfo& core_info,
    const std::string& given_name,
    const std::string& hosted_domain = kNoHostedDomainFound) {
  AccountInfo account_info;
  account_info.email = core_info.email;
  account_info.gaia = core_info.gaia;
  account_info.account_id = core_info.account_id;
  account_info.is_under_advanced_protection =
      core_info.is_under_advanced_protection;
  account_info.full_name = base::StrCat({given_name, " Doe"});
  account_info.given_name = given_name;
  account_info.hosted_domain = hosted_domain;
  account_info.locale = "en";
  account_info.picture_url = base::StrCat({"https://picture.url/", given_name});
  return account_info;
}

}  // namespace

class ProfileNameResolverTest : public testing::Test {
 protected:
  const std::string kTestGivenName = "Jane";
  const std::string kTestEmail = "jane@bar.baz";

  signin::IdentityTestEnvironment* identity_test_env() {
    return &identity_test_env_;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
};

TEST_F(ProfileNameResolverTest, RunWithProfileName) {
  CoreAccountInfo core_account_info =
      identity_test_env()->MakePrimaryAccountAvailable(
          kTestEmail, signin::ConsentLevel::kSignin);
  base::test::TestFuture<std::u16string> profile_name_future;
  base::test::TestFuture<std::u16string> profile_name_future_2;

  ProfileNameResolver resolver{identity_test_env()->identity_manager()};

  // `RunWithProfileName` should not run the callback as no profile name is
  // available.
  resolver.RunWithProfileName(profile_name_future.GetCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(profile_name_future.IsReady());

  // Simulate the account info being updated, should result in the callback
  // getting called.
  resolver.OnExtendedAccountInfoUpdated(
      FillAccountInfo(core_account_info, kTestGivenName));
  EXPECT_EQ(base::ASCIIToUTF16(kTestGivenName), profile_name_future.Get());

  // Calling `RunWithProfileName` again should make it get called right away,
  // synchronously.
  resolver.RunWithProfileName(profile_name_future_2.GetCallback());
  EXPECT_TRUE(profile_name_future_2.IsReady());
  EXPECT_EQ(base::ASCIIToUTF16(kTestGivenName), profile_name_future_2.Get());
}

TEST_F(ProfileNameResolverTest, RunWithProfileName_InfoAvailable) {
  CoreAccountInfo core_account_info =
      identity_test_env()->MakePrimaryAccountAvailable(
          kTestEmail, signin::ConsentLevel::kSignin);
  identity_test_env()->UpdateAccountInfoForAccount(
      FillAccountInfo(core_account_info, kTestGivenName));
  base::test::TestFuture<std::u16string> profile_name_future;

  ProfileNameResolver resolver{identity_test_env()->identity_manager()};

  // The information is available, the callback should run right away.
  resolver.RunWithProfileName(profile_name_future.GetCallback());
  EXPECT_TRUE(profile_name_future.IsReady());
  EXPECT_EQ(base::ASCIIToUTF16(kTestGivenName), profile_name_future.Get());
}

TEST_F(ProfileNameResolverTest, RunWithProfileName_InvalidInfo) {
  CoreAccountInfo core_account_info =
      identity_test_env()->MakePrimaryAccountAvailable(
          kTestEmail, signin::ConsentLevel::kSignin);
  auto scoped_timeout_override =
      ProfileNameResolver::CreateScopedInfoFetchTimeoutOverrideForTesting(
          base::TimeDelta());

  base::test::TestFuture<std::u16string> profile_name_future;

  ProfileNameResolver resolver{identity_test_env()->identity_manager()};
  resolver.RunWithProfileName(profile_name_future.GetCallback());

  // Simulate the account info being updated with invalid info. Should result in
  // the callback not getting called, and waiting until the timeout to get a
  // fallback value as name.
  resolver.OnExtendedAccountInfoUpdated(AccountInfo());
  EXPECT_FALSE(profile_name_future.IsReady());

  EXPECT_EQ(base::ASCIIToUTF16(kTestEmail), profile_name_future.Get());
}
