// Copyright 2025 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/browser/cobalt_content_browser_client.h"

#include <memory>
#include <string>
#include <variant>

#include "base/feature_list.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "cobalt/browser/features.h"
#include "cobalt/browser/global_features.h"
#include "content/public/browser/overlay_window.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace cobalt {
namespace {

class CobaltContentBrowserClientTest : public testing::Test {
 protected:
  void SetUp() override {
    testing::Test::SetUp();
    ClearUserAgentCacheForTesting();
  }

  void TearDown() override {
    ClearUserAgentCacheForTesting();
    testing::Test::TearDown();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::DEFAULT,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED};
};

TEST_F(CobaltContentBrowserClientTest, ParseAndApplyH5vccSettingsForTesting) {
  auto* instance = GlobalFeatures::GetInstance();
  ASSERT_NE(instance, nullptr);

  ParseAndApplyH5vccSettingsForTesting("Foo=1234;Bar=Baz", instance);

  const auto& settings = instance->GetSettings();
  auto it1 = settings.find("Foo");
  ASSERT_NE(it1, settings.end());
  EXPECT_EQ(std::get<int64_t>(it1->second), 1234);

  auto it2 = settings.find("Bar");
  ASSERT_NE(it2, settings.end());
  EXPECT_EQ(std::get<std::string>(it2->second), "Baz");
}

TEST_F(CobaltContentBrowserClientTest,
       CreateWindowForVideoPictureInPicturePlatformBehavior) {
  CobaltContentBrowserClient client(/*startup_timestamp=*/absl::nullopt,
                                    /*deep_link=*/"",
                                    /*is_visible=*/true);
  std::unique_ptr<content::VideoOverlayWindow> window =
      client.CreateWindowForVideoPictureInPicture(/*controller=*/nullptr);
// TODO: b/532158001 - Support PiP on Linux.
#if BUILDFLAG(IS_ANDROID)
  EXPECT_NE(window, nullptr);
#else
  EXPECT_EQ(window, nullptr);
#endif
}

TEST_F(CobaltContentBrowserClientTest,
       GetUserAgentDefersCachingUntilFeatureListInitialized) {
  CobaltContentBrowserClient client(/*startup_timestamp=*/absl::nullopt,
                                    /*deep_link=*/"",
                                    /*is_visible=*/true);
  base::HistogramTester histogram_tester;

  // Simulate pre-FeatureList state where FeatureList is not yet initialized.
  SetFeatureListInitializedForTesting(false);

  // When FeatureList is not initialized, dynamic UA is returned without Finch
  // token, static caching is deferred, and pre-initialization calls are
  // accumulated without logging to UMA yet.
  std::string ua_pre1 = client.GetUserAgent();
  EXPECT_FALSE(ua_pre1.empty());
  EXPECT_EQ(ua_pre1.find("Finch/"), std::string::npos);
  histogram_tester.ExpectTotalCount("Cobalt.UserAgent.PreFeatureListCallCount",
                                    0);

  std::string ua_pre2 = client.GetUserAgent();
  EXPECT_FALSE(ua_pre2.empty());
  histogram_tester.ExpectTotalCount("Cobalt.UserAgent.PreFeatureListCallCount",
                                    0);

  // Transition to initialized FeatureList with Finch token feature enabled.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kEnableUserAgentFinchToken,
      {{features::kUserAgentFinchTokenParam.name, "finch_test_token"}});
  SetFeatureListInitializedForTesting(true);

  // With FeatureList initialized, UA contains the Finch token, is cached,
  // and the accumulated pre-FeatureList call count (2) is logged exactly
  // once.
  std::string ua_post1 = client.GetUserAgent();
  EXPECT_FALSE(ua_post1.empty());
  EXPECT_NE(ua_post1.find("Finch/finch_test_token"), std::string::npos);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.UserAgent.PreFeatureListCallCount", /*sample=*/2,
      /*expected_bucket_count=*/1);

  // Subsequent calls return the cached UA string without additional UMA
  // logging.
  std::string ua_post2 = client.GetUserAgent();
  EXPECT_EQ(ua_post1, ua_post2);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.UserAgent.PreFeatureListCallCount", /*sample=*/2,
      /*expected_bucket_count=*/1);
}

TEST_F(CobaltContentBrowserClientTest,
       GetUserAgentLogsZeroPreFeatureListCallsWhenInitializedImmediately) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kEnableUserAgentFinchToken,
      {{features::kUserAgentFinchTokenParam.name, "finch_test_token"}});

  CobaltContentBrowserClient client(/*startup_timestamp=*/absl::nullopt,
                                    /*deep_link=*/"",
                                    /*is_visible=*/true);
  base::HistogramTester histogram_tester;

  std::string ua_post1 = client.GetUserAgent();
  EXPECT_FALSE(ua_post1.empty());
  EXPECT_NE(ua_post1.find("Finch/finch_test_token"), std::string::npos);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.UserAgent.PreFeatureListCallCount", /*sample=*/0,
      /*expected_bucket_count=*/1);

  // Subsequent calls return the cached UA string without additional UMA
  // logging.
  std::string ua_post2 = client.GetUserAgent();
  EXPECT_EQ(ua_post1, ua_post2);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.UserAgent.PreFeatureListCallCount", /*sample=*/0,
      /*expected_bucket_count=*/1);
}

}  // namespace
}  // namespace cobalt
