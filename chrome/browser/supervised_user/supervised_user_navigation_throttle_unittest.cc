// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_navigation_throttle.h"
#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "components/supervised_user/core/browser/supervised_user_url_filter.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "components/supervised_user/test_support/kids_management_api_server_mock.h"
#include "components/supervised_user/test_support/supervised_user_url_filter_test_utils.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace supervised_user {

namespace {
static const char* kExampleURL = "https://example.com/";

class MockSupervisedUserURLFilter
    : public supervised_user::SupervisedUserURLFilter {
 public:
  MockSupervisedUserURLFilter()
      : supervised_user::SupervisedUserURLFilter(
            base::BindRepeating([](const GURL& url) { return false; }),
            std::make_unique<supervised_user::FakeURLFilterDelegate>()) {}

  MOCK_METHOD(FilteringBehavior,
              GetFilteringBehaviorForURL,
              (const GURL& url,
               supervised_user::FilteringBehaviorReason* reason),
              (override));
};
}  // namespace

class SupervisedUserNavigationThrottleTest
    : public ChromeRenderViewHostTestHarness {
 public:
  SupervisedUserNavigationThrottleTest() {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
    feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {supervised_user::kFilterWebsitesForSupervisedUsersOnDesktopAndIOS},
        /*disabled_features=*/{});
#endif
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    profile()->SetIsSupervisedProfile();
    ASSERT_TRUE(profile()->IsChild());
  }

  std::unique_ptr<SupervisedUserNavigationThrottle> CreateNavigationThrottle(
      const GURL& url) {
    navigation_handle_ =
        std::make_unique<::testing::NiceMock<content::MockNavigationHandle>>(
            url, main_rfh());
    return SupervisedUserNavigationThrottle::MaybeCreateThrottleFor(
        navigation_handle_.get());
  }

  supervised_user::SupervisedUserURLFilter* GetSupervisedUserURLFilter() {
    return SupervisedUserServiceFactory::GetForProfile(profile())
        ->GetURLFilter();
  }

  base::HistogramTester* histogram_tester() { return &histogram_tester_; }

 private:
  std::unique_ptr<content::MockNavigationHandle> navigation_handle_;
  base::test::ScopedFeatureList feature_list_;
  base::HistogramTester histogram_tester_;
};

TEST_F(SupervisedUserNavigationThrottleTest, AllowedUrlsRecordedInAllowBucket) {
  CreateNavigationThrottle(GURL(kExampleURL))->WillStartRequest();

  histogram_tester()->ExpectBucketCount(
      supervised_user::kSupervisedUserTopLevelURLFilteringResultHistogramName,
      supervised_user::SupervisedUserFilterTopLevelResult::kAllow, 1);
}

TEST_F(SupervisedUserNavigationThrottleTest,
       BlocklistedUrlsRecordedInBlockManualBucket) {
  GURL blocked_url(kExampleURL);
  std::map<std::string, bool> hosts;
  hosts[blocked_url.host()] = false;
  GetSupervisedUserURLFilter()->SetManualHosts(std::move(hosts));
  ASSERT_EQ(
      supervised_user::SupervisedUserURLFilter::FilteringBehavior::BLOCK,
      GetSupervisedUserURLFilter()->GetFilteringBehaviorForURL(blocked_url));

  CreateNavigationThrottle(blocked_url)->WillStartRequest();

  histogram_tester()->ExpectBucketCount(
      supervised_user::kSupervisedUserTopLevelURLFilteringResultHistogramName,
      supervised_user::SupervisedUserFilterTopLevelResult::kBlockManual, 1);
}

TEST_F(SupervisedUserNavigationThrottleTest,
       AllSitesBlockedRecordedInBlockNotInAllowlistBucket) {
  GetSupervisedUserURLFilter()->SetDefaultFilteringBehavior(
      supervised_user::SupervisedUserURLFilter::BLOCK);

  CreateNavigationThrottle(GURL(kExampleURL))->WillStartRequest();

  histogram_tester()->ExpectBucketCount(
      supervised_user::kSupervisedUserTopLevelURLFilteringResultHistogramName,
      supervised_user::SupervisedUserFilterTopLevelResult::kBlockNotInAllowlist,
      1);
}

TEST_F(SupervisedUserNavigationThrottleTest,
       BlockedMatureSitesRecordedInBlockSafeSitesBucket) {
  std::unique_ptr<MockSupervisedUserURLFilter> mock_url_filter =
      std::make_unique<MockSupervisedUserURLFilter>();
  ON_CALL(*mock_url_filter, GetFilteringBehaviorForURL(testing::_, testing::_))
      .WillByDefault([](const GURL& url,
                        supervised_user::FilteringBehaviorReason* reason) {
        *reason = supervised_user::FilteringBehaviorReason::ASYNC_CHECKER;
        return supervised_user::SupervisedUserURLFilter::FilteringBehavior::
            BLOCK;
      });
  SupervisedUserServiceFactory::GetForProfile(profile())
      ->SetURLFilterForTesting(std::move(mock_url_filter));

  CreateNavigationThrottle(GURL(kExampleURL))->WillStartRequest();

  histogram_tester()->ExpectBucketCount(
      supervised_user::kSupervisedUserTopLevelURLFilteringResultHistogramName,
      supervised_user::SupervisedUserFilterTopLevelResult::kBlockSafeSites, 1);
}

}  // namespace supervised_user
