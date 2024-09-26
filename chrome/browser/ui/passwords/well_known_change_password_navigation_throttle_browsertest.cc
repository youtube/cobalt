// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/well_known_change_password_navigation_throttle.h"

#include <map>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/password_manager/affiliation_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/cert_verifier_browser_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/common/url_constants.h"
#include "components/password_manager/content/browser/password_change_success_tracker_factory.h"
#include "components/password_manager/core/browser/affiliation/affiliation_api.pb.h"
#include "components/password_manager/core/browser/affiliation/affiliation_service_impl.h"
#include "components/password_manager/core/browser/affiliation/hash_affiliation_fetcher.h"
#include "components/password_manager/core/browser/affiliation/mock_affiliation_service.h"
#include "components/password_manager/core/browser/mock_password_change_success_tracker.h"
#include "components/password_manager/core/browser/password_change_success_tracker.h"
#include "components/password_manager/core/browser/well_known_change_password_util.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/sync/test/test_sync_service.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "net/cert/x509_certificate.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {
using content::NavigationThrottle;
using content::RenderFrameHost;
using content::TestNavigationObserver;
using content::URLLoaderInterceptor;
using net::test_server::BasicHttpResponse;
using net::test_server::DelayedHttpResponse;
using net::test_server::EmbeddedTestServer;
using net::test_server::EmbeddedTestServerHandle;
using net::test_server::HttpRequest;
using net::test_server::HttpResponse;
using password_manager::FacetURI;
using password_manager::kWellKnownChangePasswordPath;
using password_manager::kWellKnownNotExistingResourcePath;
using password_manager::MockAffiliationService;
using password_manager::MockPasswordChangeSuccessTracker;
using password_manager::PasswordChangeSuccessTracker;
using password_manager::PasswordChangeSuccessTrackerFactory;
using password_manager::WellKnownChangePasswordResult;
using testing::_;
using testing::Return;

constexpr char kMockChangePasswordPath[] = "/change-password-override";

// ServerResponse describes how a server should respond to a given path.
struct ServerResponse {
  net::HttpStatusCode status_code;
  std::vector<std::pair<std::string, std::string>> headers;
  int resolve_time_in_milliseconds;
};

// The NavigationThrottle is making 2 requests in parallel. With this config we
// simulate the different orders for the arrival of the responses. The value
// represents the delay in milliseconds.
struct ResponseDelayParams {
  int change_password_delay;
  int not_exist_delay;
};

}  // namespace

class ChangePasswordNavigationThrottleBrowserTestBase
    : public CertVerifierBrowserTest,
      public testing::WithParamInterface<
          std::tuple<ui::PageTransition, ResponseDelayParams>> {
 public:
  using UkmBuilder =
      ukm::builders::PasswordManager_WellKnownChangePasswordResult;
  ChangePasswordNavigationThrottleBrowserTestBase() {
    test_server_->RegisterRequestHandler(base::BindRepeating(
        &ChangePasswordNavigationThrottleBrowserTestBase::HandleRequest,
        base::Unretained(this)));
  }

  void Initialize() {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(test_server_->Start());
    test_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  void ExpectUmaAndUkmMetric(WellKnownChangePasswordResult expected) {
    histogram_tester_->ExpectUniqueSample(
        "PasswordManager.WellKnownChangePasswordResult", expected, 1u);

    auto entries = test_recorder_->GetEntriesByName(UkmBuilder::kEntryName);
    // Expect one recorded metric.
    ASSERT_EQ(1, static_cast<int>(entries.size()));
    test_recorder_->ExpectEntryMetric(
        entries[0], UkmBuilder::kWellKnownChangePasswordResultName,
        static_cast<int64_t>(expected));
  }

  void ExpectNeitherUmaNorUkmMetric() {
    histogram_tester_->ExpectTotalCount(
        "PasswordManager.WellKnownChangePasswordResult", 0u);
    EXPECT_TRUE(
        test_recorder_->GetEntriesByName(UkmBuilder::kEntryName).empty());
  }

  ui::PageTransition page_transition() const { return std::get<0>(GetParam()); }
  ResponseDelayParams response_delays() const {
    return std::get<1>(GetParam());
  }

 protected:
  // Navigates to |navigate_url| from the mock server using |transition|. It
  // waits until the navigation to |expected_url| happened.
  void TestNavigationThrottle(
      const GURL& navigate_url,
      const GURL& expected_url,
      absl::optional<url::Origin> initiator_origin = absl::nullopt);

  // Whitelist all https certs for the |test_server_|.
  void AddHttpsCertificate() {
    auto cert = test_server_->GetCertificate();
    net::CertVerifyResult verify_result;
    verify_result.cert_status = 0;
    verify_result.verified_cert = cert;
    mock_cert_verifier()->AddResultForCert(cert.get(), verify_result, net::OK);
  }

  // Maps a path to a ServerResponse config object.
  base::flat_map<std::string, ServerResponse> path_response_map_;
  std::unique_ptr<EmbeddedTestServer> test_server_ =
      std::make_unique<EmbeddedTestServer>(EmbeddedTestServer::TYPE_HTTPS);
  base::test::ScopedFeatureList feature_list_;

 private:
  // Returns a response for the given request. Uses |path_response_map_| to
  // construct the response. Returns nullptr when the path is not defined in
  // |path_response_map_|.
  std::unique_ptr<HttpResponse> HandleRequest(const HttpRequest& request);
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_recorder_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

std::unique_ptr<HttpResponse>
ChangePasswordNavigationThrottleBrowserTestBase::HandleRequest(
    const HttpRequest& request) {
  GURL absolute_url = test_server_->GetURL(request.relative_url);
  std::string path = absolute_url.path();
  auto it = path_response_map_.find(absolute_url.path_piece());
  if (it == path_response_map_.end())
    return nullptr;
  const ServerResponse& config = it->second;
  auto http_response = std::make_unique<DelayedHttpResponse>(
      base::Milliseconds(config.resolve_time_in_milliseconds));
  http_response->set_code(config.status_code);
  http_response->set_content_type("text/plain");
  for (auto header_pair : config.headers) {
    http_response->AddCustomHeader(header_pair.first, header_pair.second);
  }
  return http_response;
}

void ChangePasswordNavigationThrottleBrowserTestBase::TestNavigationThrottle(
    const GURL& navigate_url,
    const GURL& expected_url,
    absl::optional<url::Origin> initiator_origin) {
  AddHttpsCertificate();

  NavigateParams params(browser(), navigate_url, page_transition());
  params.initiator_origin = std::move(initiator_origin);
  TestNavigationObserver observer(expected_url);
  observer.WatchExistingWebContents();
  Navigate(&params);
  observer.Wait();

  EXPECT_EQ(observer.last_navigation_url(), expected_url);
}

// Browser Test that checks navigation to /.well-known/change-password path and
// redirection to change password URL returned by Change Password Service.
class WellKnownChangePasswordNavigationThrottleBrowserTest
    : public ChangePasswordNavigationThrottleBrowserTestBase {
 public:
  WellKnownChangePasswordNavigationThrottleBrowserTest() = default;

  void SetUpOnMainThread() override;
  void TestNavigationThrottleForLocalhost(const std::string& expected_path);

  raw_ptr<MockAffiliationService, DanglingUntriaged> url_service_ = nullptr;
  raw_ptr<MockPasswordChangeSuccessTracker, DanglingUntriaged>
      password_change_success_tracker_ = nullptr;

 private:
};

void WellKnownChangePasswordNavigationThrottleBrowserTest::SetUpOnMainThread() {
  Initialize();
  url_service_ =
      AffiliationServiceFactory::GetInstance()->SetTestingSubclassFactoryAndUse(
          browser()->profile(),
          base::BindRepeating([](content::BrowserContext*) {
            return std::make_unique<
                testing::NiceMock<MockAffiliationService>>();
          }));

  password_change_success_tracker_ =
      PasswordChangeSuccessTrackerFactory::GetInstance()
          ->SetTestingSubclassFactoryAndUse(
              browser()->profile(),
              base::BindRepeating([](content::BrowserContext*) {
                return std::make_unique<
                    testing::NiceMock<MockPasswordChangeSuccessTracker>>();
              }));
}

void WellKnownChangePasswordNavigationThrottleBrowserTest::
    TestNavigationThrottleForLocalhost(const std::string& expected_path) {
  GURL navigate_url = test_server_->GetURL(kWellKnownChangePasswordPath);
  GURL expected_url = test_server_->GetURL(expected_path);

  TestNavigationThrottle(navigate_url, expected_url);
}

IN_PROC_BROWSER_TEST_P(WellKnownChangePasswordNavigationThrottleBrowserTest,
                       SupportForChangePassword) {
  path_response_map_[kWellKnownChangePasswordPath] = {
      net::HTTP_OK, {}, response_delays().change_password_delay};
  path_response_map_[kWellKnownNotExistingResourcePath] = {
      net::HTTP_NOT_FOUND, {}, response_delays().not_exist_delay};

  if (page_transition() & ui::PAGE_TRANSITION_FROM_API) {
    EXPECT_CALL(
        *password_change_success_tracker_,
        OnChangePasswordFlowModified(
            test_server_->GetURL(kWellKnownChangePasswordPath),
            PasswordChangeSuccessTracker::StartEvent::kManualWellKnownUrlFlow));
  } else {
    EXPECT_CALL(*password_change_success_tracker_,
                OnChangePasswordFlowModified(_, _))
        .Times(0);
  }

  TestNavigationThrottleForLocalhost(
      /*expected_path=*/kWellKnownChangePasswordPath);
  if (page_transition() & ui::PAGE_TRANSITION_FROM_API) {
    ExpectUmaAndUkmMetric(
        WellKnownChangePasswordResult::kUsedWellKnownChangePassword);
  } else {
    ExpectNeitherUmaNorUkmMetric();
  }
}

IN_PROC_BROWSER_TEST_P(WellKnownChangePasswordNavigationThrottleBrowserTest,
                       SupportForChangePassword_WithRedirect) {
  path_response_map_[kWellKnownChangePasswordPath] = {
      net::HTTP_PERMANENT_REDIRECT,
      {std::make_pair("Location", "/change-password")},
      response_delays().change_password_delay};
  path_response_map_[kWellKnownNotExistingResourcePath] = {
      net::HTTP_NOT_FOUND, {}, response_delays().not_exist_delay};
  path_response_map_["/change-password"] = {net::HTTP_OK, {}, 0};

  if (page_transition() & ui::PAGE_TRANSITION_FROM_API) {
    EXPECT_CALL(
        *password_change_success_tracker_,
        OnChangePasswordFlowModified(
            test_server_->GetURL(kWellKnownChangePasswordPath),
            PasswordChangeSuccessTracker::StartEvent::kManualWellKnownUrlFlow));
  } else {
    EXPECT_CALL(*password_change_success_tracker_,
                OnChangePasswordFlowModified(_, _))
        .Times(0);
  }

  TestNavigationThrottleForLocalhost(/*expected_path=*/"/change-password");
  if (page_transition() & ui::PAGE_TRANSITION_FROM_API) {
    ExpectUmaAndUkmMetric(
        WellKnownChangePasswordResult::kUsedWellKnownChangePassword);
  } else {
    ExpectNeitherUmaNorUkmMetric();
  }
}

// Tests that the throttle behaves correctly for all types of page transitions
// when initiated from within Chrome settings.
IN_PROC_BROWSER_TEST_P(
    WellKnownChangePasswordNavigationThrottleBrowserTest,
    SupportForChangePassword_WithRedirect_FromChromeSettings) {
  path_response_map_[kWellKnownChangePasswordPath] = {
      net::HTTP_PERMANENT_REDIRECT,
      {std::make_pair("Location", "/change-password")},
      response_delays().change_password_delay};
  path_response_map_[kWellKnownNotExistingResourcePath] = {
      net::HTTP_NOT_FOUND, {}, response_delays().not_exist_delay};
  path_response_map_["/change-password"] = {net::HTTP_OK, {}, 0};

  GURL navigate_url = test_server_->GetURL(kWellKnownChangePasswordPath);
  GURL expected_url = test_server_->GetURL("/change-password");

  EXPECT_CALL(
      *password_change_success_tracker_,
      OnChangePasswordFlowModified(
          navigate_url,
          PasswordChangeSuccessTracker::StartEvent::kManualWellKnownUrlFlow));

  TestNavigationThrottle(
      navigate_url, expected_url,
      url::Origin::Create(GURL("chrome://settings/passwords/check")));

  ExpectUmaAndUkmMetric(
      WellKnownChangePasswordResult::kUsedWellKnownChangePassword);
}

// Tests that the throttle behaves correctly for all types of page transitions
// when initiated from p.g.c.'s checkup.
IN_PROC_BROWSER_TEST_P(
    WellKnownChangePasswordNavigationThrottleBrowserTest,
    SupportForChangePassword_WithRedirect_FromPasswordCheckup) {
  path_response_map_[kWellKnownChangePasswordPath] = {
      net::HTTP_PERMANENT_REDIRECT,
      {std::make_pair("Location", "/change-password")},
      response_delays().change_password_delay};
  path_response_map_[kWellKnownNotExistingResourcePath] = {
      net::HTTP_NOT_FOUND, {}, response_delays().not_exist_delay};
  path_response_map_["/change-password"] = {net::HTTP_OK, {}, 0};

  GURL navigate_url = test_server_->GetURL(kWellKnownChangePasswordPath);
  GURL expected_url = test_server_->GetURL("/change-password");

  EXPECT_CALL(
      *password_change_success_tracker_,
      OnChangePasswordFlowModified(
          navigate_url,
          PasswordChangeSuccessTracker::StartEvent::kManualWellKnownUrlFlow));

  TestNavigationThrottle(
      navigate_url, expected_url,
      url::Origin::Create(GURL("https://passwords.google.com/checkup")));

  ExpectUmaAndUkmMetric(
      WellKnownChangePasswordResult::kUsedWellKnownChangePassword);
}

IN_PROC_BROWSER_TEST_P(WellKnownChangePasswordNavigationThrottleBrowserTest,
                       SupportForChangePassword_PartialContent) {
  path_response_map_[kWellKnownChangePasswordPath] = {
      net::HTTP_PARTIAL_CONTENT, {}, response_delays().change_password_delay};
  path_response_map_[kWellKnownNotExistingResourcePath] = {
      net::HTTP_NOT_FOUND, {}, response_delays().not_exist_delay};

  if (page_transition() & ui::PAGE_TRANSITION_FROM_API) {
    EXPECT_CALL(
        *password_change_success_tracker_,
        OnChangePasswordFlowModified(
            test_server_->GetURL(kWellKnownChangePasswordPath),
            PasswordChangeSuccessTracker::StartEvent::kManualWellKnownUrlFlow));
  } else {
    EXPECT_CALL(*password_change_success_tracker_,
                OnChangePasswordFlowModified(_, _))
        .Times(0);
  }

  TestNavigationThrottleForLocalhost(
      /*expected_path=*/kWellKnownChangePasswordPath);
  if (page_transition() & ui::PAGE_TRANSITION_FROM_API) {
    ExpectUmaAndUkmMetric(
        WellKnownChangePasswordResult::kUsedWellKnownChangePassword);
  } else {
    ExpectNeitherUmaNorUkmMetric();
  }
}

IN_PROC_BROWSER_TEST_P(
    WellKnownChangePasswordNavigationThrottleBrowserTest,
    SupportForChangePassword_WithXOriginRedirectToNotFoundPage) {
  GURL change_password_url =
      test_server_->GetURL("example.com", "/change-password");
  path_response_map_[kWellKnownChangePasswordPath] = {
      net::HTTP_PERMANENT_REDIRECT,
      {std::make_pair("Location", change_password_url.spec())},
      response_delays().change_password_delay};
  path_response_map_[kWellKnownNotExistingResourcePath] = {
      net::HTTP_PERMANENT_REDIRECT,
      {std::make_pair("Location", "/not-found")},
      response_delays().not_exist_delay};
  path_response_map_["/change-password"] = {net::HTTP_OK, {}, 0};
  path_response_map_["/not-found"] = {net::HTTP_NOT_FOUND, {}, 0};

  if (page_transition() & ui::PAGE_TRANSITION_FROM_API) {
    EXPECT_CALL(
        *password_change_success_tracker_,
        OnChangePasswordFlowModified(
            test_server_->GetURL(kWellKnownChangePasswordPath),
            PasswordChangeSuccessTracker::StartEvent::kManualWellKnownUrlFlow));
  } else {
    EXPECT_CALL(*password_change_success_tracker_,
                OnChangePasswordFlowModified(_, _))
        .Times(0);
  }

  TestNavigationThrottle(test_server_->GetURL(kWellKnownChangePasswordPath),
                         change_password_url);
  if (page_transition() & ui::PAGE_TRANSITION_FROM_API) {
    ExpectUmaAndUkmMetric(
        WellKnownChangePasswordResult::kUsedWellKnownChangePassword);
  } else {
    ExpectNeitherUmaNorUkmMetric();
  }
}

IN_PROC_BROWSER_TEST_P(WellKnownChangePasswordNavigationThrottleBrowserTest,
                       NoSupportForChangePassword_NotFound) {
  path_response_map_[kWellKnownChangePasswordPath] = {
      net::HTTP_NOT_FOUND, {}, response_delays().change_password_delay};
  path_response_map_[kWellKnownNotExistingResourcePath] = {
      net::HTTP_NOT_FOUND, {}, response_delays().not_exist_delay};

  if (page_transition() & ui::PAGE_TRANSITION_FROM_API) {
    EXPECT_CALL(*url_service_, GetChangePasswordURL(test_server_->GetURL(
                                   kWellKnownChangePasswordPath)))
        .WillRepeatedly(Return(GURL()));
    EXPECT_CALL(
        *password_change_success_tracker_,
        OnChangePasswordFlowModified(
            test_server_->GetURL(kWellKnownChangePasswordPath),
            PasswordChangeSuccessTracker::StartEvent::kManualHomepageFlow));
    TestNavigationThrottleForLocalhost(/*expected_path=*/"/");
    ExpectUmaAndUkmMetric(WellKnownChangePasswordResult::kFallbackToOriginUrl);
  } else {
    EXPECT_CALL(*password_change_success_tracker_,
                OnChangePasswordFlowModified(_, _))
        .Times(0);
    TestNavigationThrottleForLocalhost(
        /*expected_path=*/kWellKnownChangePasswordPath);
    ExpectNeitherUmaNorUkmMetric();
  }
}

IN_PROC_BROWSER_TEST_P(WellKnownChangePasswordNavigationThrottleBrowserTest,
                       NoSupportForChangePassword_WithUrlOverride) {
  path_response_map_[kWellKnownChangePasswordPath] = {
      net::HTTP_NOT_FOUND, {}, response_delays().change_password_delay};
  path_response_map_[kWellKnownNotExistingResourcePath] = {
      net::HTTP_NOT_FOUND, {}, response_delays().not_exist_delay};

  if (page_transition() & ui::PAGE_TRANSITION_FROM_API) {
    EXPECT_CALL(*url_service_, GetChangePasswordURL(test_server_->GetURL(
                                   kWellKnownChangePasswordPath)))
        .WillRepeatedly(Return(test_server_->GetURL(kMockChangePasswordPath)));
    EXPECT_CALL(*password_change_success_tracker_,
                OnChangePasswordFlowModified(
                    test_server_->GetURL(kWellKnownChangePasswordPath),
                    PasswordChangeSuccessTracker::StartEvent::
                        kManualChangePasswordUrlFlow));
    TestNavigationThrottleForLocalhost(
        /*expected_path=*/kMockChangePasswordPath);
    ExpectUmaAndUkmMetric(
        WellKnownChangePasswordResult::kFallbackToOverrideUrl);
  } else {
    EXPECT_CALL(*password_change_success_tracker_,
                OnChangePasswordFlowModified(_, _))
        .Times(0);
    TestNavigationThrottleForLocalhost(
        /*expected_path=*/kWellKnownChangePasswordPath);
    ExpectNeitherUmaNorUkmMetric();
  }
}

// Single page applications often return 200 for all paths
IN_PROC_BROWSER_TEST_P(WellKnownChangePasswordNavigationThrottleBrowserTest,
                       NoSupportForChangePassword_Ok) {
  path_response_map_[kWellKnownChangePasswordPath] = {
      net::HTTP_OK, {}, response_delays().change_password_delay};
  path_response_map_[kWellKnownNotExistingResourcePath] = {
      net::HTTP_OK, {}, response_delays().not_exist_delay};

  if (page_transition() & ui::PAGE_TRANSITION_FROM_API) {
    EXPECT_CALL(*url_service_, GetChangePasswordURL(test_server_->GetURL(
                                   kWellKnownChangePasswordPath)))
        .WillRepeatedly(Return(GURL()));
    EXPECT_CALL(
        *password_change_success_tracker_,
        OnChangePasswordFlowModified(
            test_server_->GetURL(kWellKnownChangePasswordPath),
            PasswordChangeSuccessTracker::StartEvent::kManualHomepageFlow));
    TestNavigationThrottleForLocalhost(/*expected_path=*/"/");
    ExpectUmaAndUkmMetric(WellKnownChangePasswordResult::kFallbackToOriginUrl);
  } else {
    EXPECT_CALL(*password_change_success_tracker_,
                OnChangePasswordFlowModified(_, _))
        .Times(0);
    TestNavigationThrottleForLocalhost(
        /*expected_path=*/kWellKnownChangePasswordPath);
    ExpectNeitherUmaNorUkmMetric();
  }
}

IN_PROC_BROWSER_TEST_P(
    WellKnownChangePasswordNavigationThrottleBrowserTest,
    NoSupportForChangePassword_WithXOriginRedirectToNotFoundPage) {
  // Test a cross-origin redirect to a 404 page. Ensure that we try to obtain
  // the ChangePasswordUrl for the original origin.
  GURL not_found_url = test_server_->GetURL("example.com", "/not-found");
  path_response_map_[kWellKnownChangePasswordPath] = {
      net::HTTP_PERMANENT_REDIRECT,
      {std::make_pair("Location", not_found_url.spec())},
      response_delays().change_password_delay};
  path_response_map_[kWellKnownNotExistingResourcePath] = {
      net::HTTP_PERMANENT_REDIRECT,
      {std::make_pair("Location", not_found_url.spec())},
      response_delays().not_exist_delay};
  path_response_map_["/not-found"] = {net::HTTP_NOT_FOUND, {}, 0};

  if (page_transition() & ui::PAGE_TRANSITION_FROM_API) {
    EXPECT_CALL(*url_service_, GetChangePasswordURL(test_server_->GetURL(
                                   kWellKnownChangePasswordPath)))
        .WillRepeatedly(Return(GURL()));
    EXPECT_CALL(
        *password_change_success_tracker_,
        OnChangePasswordFlowModified(
            test_server_->GetURL(kWellKnownChangePasswordPath),
            PasswordChangeSuccessTracker::StartEvent::kManualHomepageFlow));
    TestNavigationThrottleForLocalhost(/*expected_path=*/"/");
    ExpectUmaAndUkmMetric(WellKnownChangePasswordResult::kFallbackToOriginUrl);
  } else {
    EXPECT_CALL(*password_change_success_tracker_,
                OnChangePasswordFlowModified(_, _))
        .Times(0);
    TestNavigationThrottle(test_server_->GetURL(kWellKnownChangePasswordPath),
                           not_found_url);
    ExpectNeitherUmaNorUkmMetric();
  }
}

IN_PROC_BROWSER_TEST_P(WellKnownChangePasswordNavigationThrottleBrowserTest,
                       NoSupportForChangePassword_WillFailRequest) {
  path_response_map_[kWellKnownChangePasswordPath] = {
      net::HTTP_PERMANENT_REDIRECT,
      {std::make_pair("Location", "/change-password")},
      response_delays().change_password_delay};
  path_response_map_[kWellKnownNotExistingResourcePath] = {
      net::HTTP_NOT_FOUND, {}, response_delays().not_exist_delay};

  // Make request fail.
  scoped_refptr<net::X509Certificate> cert = test_server_->GetCertificate();
  net::CertVerifyResult verify_result;
  verify_result.cert_status = 0;
  verify_result.verified_cert = cert;
  mock_cert_verifier()->AddResultForCert(cert.get(), verify_result,
                                         net::ERR_BLOCKED_BY_CLIENT);

  GURL url = test_server_->GetURL(kWellKnownChangePasswordPath);
  NavigateParams params(browser(), url, page_transition());
  Navigate(&params);
  TestNavigationObserver observer(params.navigated_or_inserted_contents);
  observer.Wait();

  EXPECT_EQ(observer.last_navigation_url(), url);
  // Expect no UKMs saved.
  ExpectNeitherUmaNorUkmMetric();
}

IN_PROC_BROWSER_TEST_P(WellKnownChangePasswordNavigationThrottleBrowserTest,
                       AffiliationServiceReturnsWellKnownChangePasswordPath) {
  path_response_map_[kWellKnownChangePasswordPath] = {
      net::HTTP_PERMANENT_REDIRECT,
      {std::make_pair("Location", "/change-password")},
      response_delays().change_password_delay};
  path_response_map_[kWellKnownNotExistingResourcePath] = {
      net::HTTP_OK, {}, response_delays().not_exist_delay};
  path_response_map_["/change-password"] = {net::HTTP_OK, {}, 0};

  EXPECT_CALL(
      *url_service_,
      GetChangePasswordURL(test_server_->GetURL(kWellKnownChangePasswordPath)))
      .WillRepeatedly(
          Return(test_server_->GetURL(kWellKnownChangePasswordPath)));

  GURL navigate_url = test_server_->GetURL(kWellKnownChangePasswordPath);
  GURL expected_url = test_server_->GetURL("/change-password");
  EXPECT_CALL(
      *password_change_success_tracker_,
      OnChangePasswordFlowModified(
          navigate_url,
          PasswordChangeSuccessTracker::StartEvent::kManualWellKnownUrlFlow));
  TestNavigationThrottle(
      navigate_url, expected_url,
      url::Origin::Create(GURL("https://passwords.google.com/checkup")));

  ExpectUmaAndUkmMetric(
      WellKnownChangePasswordResult::kUsedWellKnownChangePassword);
}

// Harness for testing the throttle with prerendering involved.
class PrerenderingChangePasswordNavigationThrottleBrowserTest
    : public WellKnownChangePasswordNavigationThrottleBrowserTest {
 public:
  PrerenderingChangePasswordNavigationThrottleBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &PrerenderingChangePasswordNavigationThrottleBrowserTest::
                web_contents,
            base::Unretained(this))) {}

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void SetUp() override {
    prerender_helper_.SetUp(test_server_.get());
    WellKnownChangePasswordNavigationThrottleBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override { Initialize(); }

 protected:
  content::test::PrerenderTestHelper prerender_helper_;
};

// Test the behavior of the throttle when navigated from a prerendering
// context. This is a fairly narrow use case since, of prerendering-capable
// navigations, the throttle only runs when initiated by passwords.google.com.
// However, if that origin did prerendering the well known password change URL,
// make sure we don't run the throttle as it doesn't currently support
// Prerender2. This test just ensures we don't run the throttle in this case so
// we don't get side-effects in the primary frame while prerendering the
// .well-known URL.
IN_PROC_BROWSER_TEST_P(PrerenderingChangePasswordNavigationThrottleBrowserTest,
                       EnsurePrerenderCanceled) {
  // The URL we simulate a real user navigation to.
  const GURL kNavigateUrl = GURL("https://passwords.google.com/checkup");

  // The well known password change URL. The above URL will request a prerender
  // of this page via the <script type="speculationrules"> script in the
  // response.
  // TODO(bokan): Normally the change-password URL would lead to a different
  // origin (e.g. example.com) but prerender2 doesn't yet support cross-origin
  // prerendering. Using passwords.google.com here is a bit unrealistic but
  // ensures we trigger prerendering. Once prerender2 supports cross-origin
  // prerendering this should be updated to 'example.com'.
  const GURL kWellKnownUrl =
      GURL("https://passwords.google.com/.well-known/change-password");

  URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&](URLLoaderInterceptor::RequestParams* params) {
        // We should cancel the prerender in WillStartRequest so we should
        // never receive this request.
        EXPECT_NE(params->url_request.url, kWellKnownUrl);

        // If the non-existing-resource path is requested it means the throttle
        // is running so fail the test.
        EXPECT_NE(params->url_request.url.path(),
                  kWellKnownNotExistingResourcePath);
        std::string speculation_script = base::ReplaceStringPlaceholders(
            R"(
                <script type="speculationrules">
                {
                  "prerender":[
                    {"source": "list",
                    "urls": ["$1"]}
                  ]
                }
                </script>
              )",
            {kWellKnownUrl.spec()}, nullptr);

        if (params->url_request.url == kNavigateUrl) {
          URLLoaderInterceptor::WriteResponse(
              "HTTP/1.1 200 OK\n"
              "Content-Type: text/html\n\n",
              speculation_script, params->client.get());
          return true;
        }

        return false;
      }));

  // Navigate to the passwords.google.com/checkup page. Wait until it triggers
  // a prerender from the <link> tag for the .well-known URL.
  content::test::PrerenderHostObserver observer(*web_contents(), kWellKnownUrl);
  NavigateParams params(browser(), kNavigateUrl, page_transition());
  Navigate(&params);

  // The throttle must cancel the prerendering so wait until we see the
  // prerender destroyed.
  observer.WaitForDestroyed();

  // Ensure we didn't run the throttle.
  ExpectNeitherUmaNorUkmMetric();

  // Ensure we canceled the prerender.
  EXPECT_EQ(prerender_helper_.GetHostForUrl(kWellKnownUrl),
            RenderFrameHost::kNoFrameTreeNodeId);
}

constexpr ResponseDelayParams kDelayParams[] = {{0, 1}, {1, 0}};

INSTANTIATE_TEST_SUITE_P(
    All,
    WellKnownChangePasswordNavigationThrottleBrowserTest,
    ::testing::Combine(::testing::Values(ui::PAGE_TRANSITION_LINK,
                                         ui::PAGE_TRANSITION_FROM_API),
                       ::testing::ValuesIn(kDelayParams)));

INSTANTIATE_TEST_SUITE_P(
    All,
    PrerenderingChangePasswordNavigationThrottleBrowserTest,
    ::testing::Combine(::testing::Values(ui::PAGE_TRANSITION_FROM_API),
                       ::testing::ValuesIn(kDelayParams)));
