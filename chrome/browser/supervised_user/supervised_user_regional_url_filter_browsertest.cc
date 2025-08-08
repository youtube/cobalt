// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

#include "base/callback_list.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/types/strong_alias.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ssl/https_upgrades_util.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/supervised_user/supervision_mixin.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/supervised_user/core/browser/family_link_user_capabilities.h"
#include "components/supervised_user/core/browser/fetcher_config.h"
#include "components/supervised_user/core/browser/proto/kidsmanagement_messages.pb.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/test_support/kids_management_api_server_mock.h"
#include "components/variations/variations_switches.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "services/network/public/cpp/network_switches.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace supervised_user {
namespace {

using ::kidsmanagement::ClassifyUrlRequest;
using ::testing::_;
using ::testing::AllOf;

// Wrapper class; introducing fluent aliases for test parameters.
class TestCase {
 public:
  explicit TestCase(const SupervisionMixin::SignInMode test_case_base)
      : test_case_base_(test_case_base) {}

  // Named accessors to TestCase's objects.
  SupervisionMixin::SignInMode GetSignInMode() const { return test_case_base_; }

 private:
  SupervisionMixin::SignInMode test_case_base_;
};

// The region code for variations service (any should work).
constexpr std::string_view kRegionCode = "jp";

// Tests custom filtering logic based on regions, for supervised users.
class SupervisedUserRegionalURLFilterTest
    : public MixinBasedInProcessBrowserTest,
      public ::testing::WithParamInterface<SupervisionMixin::SignInMode> {
 public:
  SupervisedUserRegionalURLFilterTest() = default;
  ~SupervisedUserRegionalURLFilterTest() override = default;

 protected:
  static const TestCase GetTestCase() { return TestCase(GetParam()); }
  void SetUpCommandLine(base::CommandLine* command_line) override {
    MixinBasedInProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        variations::switches::kVariationsOverrideCountry, kRegionCode);
  }

 protected:
  supervised_user::KidsManagementApiServerMock& kids_management_api_mock() {
    return supervision_mixin_.api_mock_setup_mixin().api_mock();
  }

  bool IsUrlFilteringEnabled() const {
    return signin::Tribool::kTrue ==
           supervised_user::IsPrimaryAccountSubjectToParentalControls(
               IdentityManagerFactory::GetForProfile(browser()->profile()));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  supervised_user::SupervisionMixin supervision_mixin_{
      mixin_host_,
      this,
      embedded_test_server(),
      {
          .sign_in_mode = GetTestCase().GetSignInMode(),
          .embedded_test_server_options =
              {
                  .resolver_rules_map_host_list =
                      "*.example.com",  // example.com must be resolved, because
                                        // the in proc browser is requesting it,
                                        // and otherwise tests timeout.
              },
      }};
};

// Verifies that the regional setting is passed to the RPC backend.
IN_PROC_BROWSER_TEST_P(SupervisedUserRegionalURLFilterTest, RegionIsAdded) {
  ScopedAllowHttpForHostnamesForTesting allow_http(
      {"www.example.com"}, browser()->profile()->GetPrefs());

  std::string url_to_classify =
      "http://www.example.com/simple.html";  // Hostname of this url must be
                                             // resolved to embedded test
                                             // server's address.
  if (IsUrlFilteringEnabled()) {
    kids_management_api_mock().AllowSubsequentClassifyUrl();
    EXPECT_CALL(
        kids_management_api_mock().classify_url_mock(),
        ClassifyUrl(AllOf(supervised_user::Classifies(url_to_classify),
                          supervised_user::SetsRegionCode(kRegionCode))))
        .Times(1);
  } else {
    EXPECT_CALL(kids_management_api_mock().classify_url_mock(), ClassifyUrl(_))
        .Times(0);
  }
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(url_to_classify)));
}

// Instead of /0, /1... print human-readable description of the test: type of
// the user signed in and the list of conditionally enabled features.
std::string PrettyPrintTestCaseName(
    const ::testing::TestParamInfo<SupervisionMixin::SignInMode>& info) {
  std::stringstream ss;
  ss << TestCase(info.param).GetSignInMode() << "Account";
  return ss.str();
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SupervisedUserRegionalURLFilterTest,
    ::testing::Values(
#if !BUILDFLAG(IS_CHROMEOS_ASH)
        // Only for platforms that support signed-out browser.
        SupervisionMixin::SignInMode::kSignedOut,
#endif
        SupervisionMixin::SignInMode::kRegular,
        SupervisionMixin::SignInMode::kSupervised),
    &PrettyPrintTestCaseName);
}  // namespace
}  // namespace supervised_user
